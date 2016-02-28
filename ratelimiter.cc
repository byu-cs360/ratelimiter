// RateLimiter by Daniel Zappala, Brigham Young University.
// This program is licensed under the GPL; see LICENSE for details.

#include <errno.h>
#include <math.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include <iostream>

using namespace std;

#include "ratelimiter.h"

RateLimiter::RateLimiter()
{
      // default rate is unlimited
    rate_ = 0;
    maxburst_ = 10000;
    clock_gettime(CLOCK_REALTIME,&send_);
    clock_gettime(CLOCK_REALTIME,&recv_);
    pthread_mutex_init(&mutex_, NULL);
}

RateLimiter::RateLimiter(int r)
{
    rate_ = r*1000;
    maxburst_ = 10000;
    clock_gettime(CLOCK_REALTIME,&send_);
    clock_gettime(CLOCK_REALTIME,&recv_);
    pthread_mutex_init(&mutex_, NULL);
}

RateLimiter::RateLimiter(int r, int maxburst)
{
    rate_ = r*1000;
    maxburst_ = maxburst;
    clock_gettime(CLOCK_REALTIME,&send_);
    pthread_mutex_init(&mutex_, NULL);
}

RateLimiter::~RateLimiter()
{
    pthread_mutex_destroy(&mutex_);
}

size_t
RateLimiter::send(int s, const void *buf, size_t len, int flags)
{
    struct timespec now, mysend;
    struct timespec t1, t2, diff;
    double duration;
    char *ptr;
    size_t total,size, result;

      // send at unlimited rate if no rate configured
    if (rate_ == 0)
        return ::send(s,buf,len,flags);

    ptr = (char *) buf;
    total = len;
    while (total > 0) {
	  // find size to send
	if (total > maxburst_)
	    size = maxburst_;
	else
	    size = total;

	  // figure ideal duration of sending
	duration = (double) (size * 8) / rate_;

	  // get current time
	clock_gettime(CLOCK_REALTIME,&now);

	  // initialize my starting time
	mysend.tv_sec = 0;
	mysend.tv_nsec = 0;

	  // begin critical section
	pthread_mutex_lock(&mutex_);

	  // handle bookkeeping to get accurate rate
	if (duration >= sendextra_) {
	    duration -= sendextra_;
	    sendextra_ = 0;
	} else {
	    sendextra_ -= duration;
	    duration = 0;
	}

	  // get my sending time and set next sending time
	if (time_less(&send_,&now))
	    time_set(&send_,&now);
	else
	    time_diff(&send_,&now,&mysend);
	time_add(&send_,duration);

	  // end critical section
	pthread_mutex_unlock(&mutex_);

	  // sleep until it is my time to send
	time_add(&mysend,duration);
	nanosleep(&mysend,NULL);

	  // send the data
	clock_gettime(CLOCK_REALTIME,&t1);
	result = sendall(s,ptr,size,flags);
	if (result < 0)
	    return result;
	clock_gettime(CLOCK_REALTIME,&t2);

	  // adjust bookkeeping
	pthread_mutex_lock(&mutex_);
	sendextra_ += time_diff2(&t2,&t1);
	pthread_mutex_unlock(&mutex_);

	total -= size;
	ptr += size;
    }
    return len;
}

size_t
RateLimiter::recv(int s, void *buf, size_t len, int flags)
{
    struct timespec now, myrecv;
    struct timespec t1, t2, diff;
    double duration;
    char *ptr;
    size_t total,size;
    int result;

      // send at unlimited rate if no rate configured
    if (rate_ == 0)
        return ::recv(s,buf,len,flags);

      // find size to receive
    if (len > maxburst_)
	size = maxburst_;
    else
	size = len;

      // get the data
    clock_gettime(CLOCK_REALTIME,&t1);
    result = ::recv(s,buf,size,flags);
    if (result <= 0)
	return result;
    clock_gettime(CLOCK_REALTIME,&t2);

      // figure ideal duration of receiving
    duration = (double) (result * 8) / rate_;

      // get current time
    clock_gettime(CLOCK_REALTIME,&now);

      // initialize my starting time
    myrecv.tv_sec = 0;
    myrecv.tv_nsec = 0;

      // begin critical section
    pthread_mutex_lock(&mutex_);
    
      // handle bookkeeping to get accurate rate
    recvextra_ += time_diff2(&t2,&t1);
    if (duration >= recvextra_) {
	duration -= recvextra_;
	recvextra_ = 0;
    } else {
	recvextra_ -= duration;
	duration = 0;
    }

      // get my receiving time and set next receiving time
    if (time_less(&recv_,&now))
	time_set(&recv_,&now);
    else
	time_diff(&recv_,&now,&myrecv);
    time_add(&recv_,duration);

      // end critical section
    pthread_mutex_unlock(&mutex_);

      // sleep until it is my time to receive
    time_add(&myrecv,duration);
    nanosleep(&myrecv,NULL);

    return result;
}

ssize_t
RateLimiter::sendfile(int sock, int fd, off_t* offset, size_t count)
{
    size_t len, rnum, snum;
    char buf[1025];

    if (rate_ == 0)
        return ::sendfile(sock,fd,offset,count);
    
    len = 0;
    while (len < count) {
          // read from file
        rnum = read(fd,buf,1024);
        if (rnum < 0) {
            if (errno == EINTR) {
                continue;
            } else {
		return rnum;
            }
	} else if (rnum == 0) {
	      // file closed before we got the desired size
	    return len;
	}
        if (rnum > (count - len))
            rnum = count - len;
        snum = send(sock,buf,rnum,0);
        if (snum < 0)
            return snum;
        len += rnum;
    }
    return count;
}

size_t
RateLimiter::sendall(int s, char *buf, size_t len, int flags)
{
    char *ptr;
    size_t nleft;
    int error = 0;
    int nwritten;

    ptr = buf;
    nleft = len;
    while (nleft) {
	if ((nwritten = ::send(s, ptr, nleft, flags)) < 0) {
	    if (errno == EINTR) {
		nwritten = 0;
	    } else {
                return error;
	    }
	} else if (nwritten == 0) {
	    break;
	}
	nleft -= nwritten;
	ptr += nwritten;
    }
    return len;
}

int
RateLimiter::time_less(struct timespec *t1, struct timespec *t2)
{
    if (t1->tv_sec < t2->tv_sec ||
	(t1->tv_sec == t2->tv_sec && t1->tv_nsec < t2->tv_nsec))
	return 1;
    else
	return 0;
}

void
RateLimiter::time_set(struct timespec *t1, struct timespec *t2)
{
    t1->tv_sec = t2->tv_sec;
    t1->tv_nsec = t2->tv_nsec;
}

void
RateLimiter::time_add(struct timespec *t1, double duration)
{
    int nmax = 1000000000;
    t1->tv_sec += (int) trunc(duration);
    t1->tv_nsec += (int) trunc((duration - trunc(duration))*nmax);
    if (t1->tv_nsec >= nmax) {
	t1->tv_sec += (t1->tv_nsec / nmax);
	t1->tv_nsec = t1->tv_nsec % nmax;
    }
}

void
RateLimiter::time_diff(struct timespec *t1, struct timespec *t2,
		       struct timespec *diff)
{
    int nmax = 1000000000;
    diff->tv_sec = t1->tv_sec - t2->tv_sec;
    diff->tv_nsec = t1->tv_nsec - t2->tv_nsec;
    if (diff->tv_nsec < 0) {
	diff->tv_sec -= 1;
	diff->tv_nsec += nmax;
    }
}

double
RateLimiter::time_diff2(struct timespec *t1, struct timespec *t2)
{
    int nmax = 1000000000;
    long sec;
    long nsec;
    double result;
    
    sec = t1->tv_sec - t2->tv_sec;
    nsec = t1->tv_nsec - t2->tv_nsec;
    if (nsec < 0) {
	sec -= 1;
	nsec = nmax + nsec;
    }
    result = sec + (double)nsec/nmax;
    return result;
}
