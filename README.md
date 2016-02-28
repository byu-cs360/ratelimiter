======================================================================
RateLimiter, by Daniel Zappala, Brigham Young University
======================================================================

This code implements a rate limiter to control the overall rate at
which an application sends and receives data.  This allows a user to
test the application as if were running on a host with an Internet
connection having an arbitrary speed.

A rate limiter instance is configured with a maximum rate in kilobits
per second (Kbps).  To use the limiter, the application calls the
recv(), send(), and sendfile() methods defined in the limiter.  The
syntax fo these calls mimics those in the standard socket API.

The rate limiter will approximately limit the receiving and sending
rates to the configured limit by scheduling events in the future to
receive or send the data.  The actual rate an application receives
will be approximately equal to the limit, but it may be bursty, rather
than smooth.

To limit burstiness, the user may specify a maximum burst size in
bytes.  For example, limiting the buffer to 100 bytes means that the
rate limiter will send 100 bytes at a time, resulting in a smoother
rate when the data is bursty.  However, smaller burst sizes increase
the scheduling overhead of the rate limiter and thus may impact its
accuracy.  In practice, the default burst size works well.

In general, the rate limiter doesn't tend to work well for speeds
higher than 1 Mbps when the server is singly threaded (the rate limiter
will provide a lower limit than what is specified).  However, with
a multiply-threaded server, the rate limiter works well for speeds
up to at least 10 Mbps.

======================================================================

Contents:

1) LICENSE - GNU GPL license for the code

2) ratelimiter.cc/.h - The rate limiter code.

Example:

// create Rate Limiter instance
RateLimiter limiter;
// set rate to 10000 Kbps or 10 Mbps
limiter.set_rate(10000);
// receive a message
int rnum = limiter->recv(client, buf, length, 0);
// send a message
int nwritten = limiter->send(client, buf, length, 0);

Notes:

You need to link in the real time clock using "-lrt" when you compile
your program.

You MUST create only one Rate Limiter instance and then have all
threads access this shared instance.

The rate limiter methods function identically to the corresponding
socket calls.  This means you still need to check return values and
the errno global variable.  You also need to use proper recv() and
send() loops.  If you have written your code correctly, you should
only need to replace all calls to send(), sendfile(), and recv() with
the corresponding rate limiter method.

