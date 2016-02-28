// RateLimiter by Daniel Zappala, Brigham Young University.
// This program is licensed under the GPL; see LICENSE for details.

#ifndef rate_limiter_h
#define rate_limiter_h

#include <pthread.h>
#include <time.h>

// This rate limiter will limit the overall rate at which the
// application sends data.  The rate is given in kilobits per second.
// We approximate this rate by scheduling a future time to send the
// data.  Thus, the actual rate an application receives will be
// bursty, not smooth.

// To limit burstiness, the user may specify a maximum burst size in bytes.
// For example, limiting the buffer to 100 bytes means that the rate limiter
// will send 100 bytes at a time, resulting in a smoother rate when the
// data is bursty.

// This rate limiter doesn't tend to work well for speeds higher than 1 Mbps.

class RateLimiter {
 public:
      // Initialize with a rate in kilobits per second (kbps) and an optional
      // max burst size (maximum bytes to send at one time).  If no
      // rate is set, then the default is unlimited.
    RateLimiter();
    RateLimiter(int);
    RateLimiter(int,int);
    ~RateLimiter();

      // Set the rate in kbps.
    inline void set_rate(int r) { rate_ = 1000*r; }
    inline void set_rate(int r, int m) { rate_ = 1000*r; maxburst_ = m;}

      // Get the current rate in bps.
    inline int get_rate() { return rate_; }

      // Send at the configured rate.  Return the number of characters
      // sent on success, otherwise -1 and errno is set to indicate
      // the exact error.
    size_t send(int,const void*,size_t,int);

      // Receive at the configured rate.  Return number of characters
      // received on success, otherwise -1 and errno is set to indicate
      // the exact error.
    size_t recv(int,void*,size_t,int);

      // Send a file over a socket.  Returns the number of characters
      // sent on success, otherwise -1 and errno is set to indicate
      // the exact error.  Ignores the offset.
    ssize_t sendfile(int, int,off_t*,size_t);
		
 private:
    size_t sendall(int,char*,size_t,int);

    void time_set(struct timespec*,struct timespec*);
    void time_add(struct timespec*,double);
    int time_less(struct timespec*,struct timespec*);
    void time_diff(struct timespec*,struct timespec*,struct timespec*);
    double time_diff2(struct timespec*,struct timespec*);

    pthread_mutex_t mutex_;
    struct timespec send_;
    struct timespec recv_;
    double sendextra_;
    double recvextra_;
    int rate_;
    int maxburst_;
};

#endif /*rate_limiter_h*/
