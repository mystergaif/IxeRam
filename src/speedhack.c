#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <sys/select.h>

typedef struct {
  double speed;
} SpeedhackConfig;

static SpeedhackConfig *config = NULL;
static double current_speed = 1.0;

// We need to track fake time separately for different clocks to ensure continuity
typedef struct {
    double last_real;
    double accumulated_fake;
    double last_speed;
    int initialized;
} ClockState;

static ClockState mono_state = {0, 0, 1.0, 0};
static ClockState real_state = {0, 0, 1.0, 0};
static pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;

// Original function pointers
static int (*orig_clock_gettime)(clockid_t clk_id, struct timespec *tp) = NULL;
static int (*orig_gettimeofday)(struct timeval *tv, void *tz) = NULL;
static int (*orig_nanosleep)(const struct timespec *req,
                             struct timespec *rem) = NULL;
static int (*orig_clock_nanosleep)(clockid_t, int, const struct timespec *,
                                   struct timespec *) = NULL;
static unsigned int (*orig_sleep)(unsigned int) = NULL;
static int (*orig_usleep)(useconds_t) = NULL;
static time_t (*orig_time)(time_t *tloc) = NULL;
static int (*orig_poll)(struct pollfd *fds, nfds_t nfds, int timeout) = NULL;
static int (*orig_select)(int nfds, fd_set *readfds, fd_set *writefds,
                          fd_set *exceptfds, struct timeval *timeout) = NULL;

static int shm_initialized = 0;

static void init_speedhack() {
  if (!orig_clock_gettime) {
    orig_clock_gettime = (int (*)(clockid_t, struct timespec *))dlsym(RTLD_NEXT, "clock_gettime");
    orig_gettimeofday = (int (*)(struct timeval *, void *))dlsym(RTLD_NEXT, "gettimeofday");
    orig_nanosleep = (int (*)(const struct timespec *, struct timespec *))dlsym(RTLD_NEXT, "nanosleep");
    orig_clock_nanosleep = (int (*)(clockid_t, int, const struct timespec *, struct timespec *))dlsym(RTLD_NEXT, "clock_nanosleep");
    orig_sleep = (unsigned int (*)(unsigned int))dlsym(RTLD_NEXT, "sleep");
    orig_usleep = (int (*)(useconds_t))dlsym(RTLD_NEXT, "usleep");
    orig_time = (time_t (*)(time_t *))dlsym(RTLD_NEXT, "time");
    orig_poll = (int (*)(struct pollfd *, nfds_t, int))dlsym(RTLD_NEXT, "poll");
    orig_select = (int (*)(int, fd_set *, fd_set *, fd_set *, struct timeval *))dlsym(RTLD_NEXT, "select");
  }

  if (shm_initialized) return;

  char shm_name[64];
  snprintf(shm_name, sizeof(shm_name), "/speedhack_%d", getpid());
  int fd = shm_open(shm_name, O_RDONLY, 0444);
  if (fd != -1) {
    void *ptr = mmap(NULL, sizeof(SpeedhackConfig), PROT_READ, MAP_SHARED, fd, 0);
    if (ptr != MAP_FAILED) {
      config = (SpeedhackConfig *)ptr;
      shm_initialized = 1;
    }
    close(fd);
  }
}

static double ts_to_double(const struct timespec *ts) {
  return (double)ts->tv_sec + (double)ts->tv_nsec / 1e9;
}
static void double_to_ts(double d, struct timespec *ts) {
  ts->tv_sec = (time_t)d;
  ts->tv_nsec = (long)((d - (double)ts->tv_sec) * 1e9);
}

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
  init_speedhack();
  int res = orig_clock_gettime(clk_id, tp);
  if (res == 0 && (clk_id == CLOCK_MONOTONIC || clk_id == CLOCK_REALTIME)) {
    pthread_mutex_lock(&time_mutex);
    
    double speed = (config && config->speed >= 0) ? config->speed : current_speed;
    ClockState *state = (clk_id == CLOCK_MONOTONIC) ? &mono_state : &real_state;
    double current_real = ts_to_double(tp);

    if (!state->initialized) {
        state->last_real = current_real;
        state->accumulated_fake = current_real;
        state->last_speed = speed;
        state->initialized = 1;
    } else {
        double delta_real = current_real - state->last_real;
        // Even if speed changed, we use the OLD speed for the delta since last call
        // for better continuity, then update to new speed.
        state->accumulated_fake += delta_real * state->last_speed;
        state->last_real = current_real;
        state->last_speed = speed;
    }
    
    double_to_ts(state->accumulated_fake, tp);
    pthread_mutex_unlock(&time_mutex);
  }
  return res;
}

int gettimeofday(struct timeval *tv, void *tz) {
  init_speedhack();
  struct timespec tp;
  int res = clock_gettime(CLOCK_REALTIME, &tp);
  if (res == 0 && tv) {
    tv->tv_sec = tp.tv_sec;
    tv->tv_usec = tp.tv_nsec / 1000;
  }
  return res;
}

time_t time(time_t *tloc) {
  init_speedhack();
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  if (tloc) *tloc = tp.tv_sec;
  return tp.tv_sec;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  init_speedhack();
  double speed = (config && config->speed >= 0) ? config->speed : current_speed;
  if (speed <= 0.0001) {
      // If speed is ~0, sleep for a long time or until interrupted
      struct timespec long_sleep = {3600, 0}; 
      return orig_nanosleep(&long_sleep, rem);
  }
  if (speed == 1.0) return orig_nanosleep(req, rem);
  
  struct timespec mreq;
  double_to_ts(ts_to_double(req) / speed, &mreq);
  return orig_nanosleep(&mreq, rem);
}

int clock_nanosleep(clockid_t clock_id, int flags,
                    const struct timespec *request, struct timespec *remain) {
  init_speedhack();
  double speed = (config && config->speed >= 0) ? config->speed : current_speed;
  if (speed <= 0.0001) {
       struct timespec long_sleep = {3600, 0};
       return orig_clock_nanosleep(clock_id, flags, &long_sleep, remain);
  }
  if (speed == 1.0) return orig_clock_nanosleep(clock_id, flags, request, remain);

  struct timespec mreq;
  double_to_ts(ts_to_double(request) / speed, &mreq);
  return orig_clock_nanosleep(clock_id, flags, &mreq, remain);
}

unsigned int sleep(unsigned int seconds) {
  struct timespec req, rem;
  req.tv_sec = seconds;
  req.tv_nsec = 0;
  nanosleep(&req, &rem);
  return (unsigned int)rem.tv_sec;
}

int usleep(useconds_t usec) {
  struct timespec req, rem;
  double_to_ts((double)usec / 1e6, &req);
  return nanosleep(&req, &rem);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    init_speedhack();
    double speed = (config && config->speed >= 0) ? config->speed : current_speed;
    if (timeout > 0 && speed > 0 && speed != 1.0) {
        timeout = (int)((double)timeout / speed);
    } else if (timeout > 0 && speed <= 0.0001) {
        timeout = -1; // Block indefinitely
    }
    return orig_poll(fds, nfds, timeout);
}

int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout) {
    init_speedhack();
    double speed = (config && config->speed >= 0) ? config->speed : current_speed;
    if (timeout && speed > 0 && speed != 1.0) {
        double t = (double)timeout->tv_sec + (double)timeout->tv_usec / 1e6;
        t /= speed;
        timeout->tv_sec = (time_t)t;
        timeout->tv_usec = (suseconds_t)((t - (double)timeout->tv_sec) * 1e6);
    } else if (timeout && speed <= 0.0001) {
        return orig_select(nfds, readfds, writefds, exceptfds, NULL);
    }
    return orig_select(nfds, readfds, writefds, exceptfds, timeout);
}
