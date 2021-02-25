#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * Copy+paste from OpenBSD's implementation in <sys/time.h>.
 */
#define timespecsub(tsp, usp, vsp)                                                                 \
    do {                                                                                           \
        (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;                                             \
        (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;                                          \
        if ((vsp)->tv_nsec < 0) {                                                                  \
            (vsp)->tv_sec--;                                                                       \
            (vsp)->tv_nsec += 1000000000L;                                                         \
        }                                                                                          \
    } while (0)

// The "do ... while (0)" is magic to make this multi-line macro work.
// <https://stackoverflow.com/questions/257418/do-while-0-what-is-it-good-for>

static size_t _block_nbytes = 1 << 12;
static size_t _total_to_write = 1 << 24;

static bool _running_inside_shadow = false;

__attribute__((constructor)) void _running_inside_shadow_init() {
    const char* shadow_pid = getenv("SHADOW_PID");

    if (shadow_pid) {
        _running_inside_shadow = true;
    }
}

enum { WARMUP_ITERATIONS = 1000, NSAMPLES = 10000 };

int asm_gettime(struct timespec* t);
void shadow_intercepted_gettime(struct timespec *t);

static void _gettime(struct timespec *t) {
  if (_running_inside_shadow) {
    shadow_intercepted_gettime(t);
  } else {
    asm_gettime(t);
  }
}

static inline double _timespec_to_seconds(const struct timespec* t) {
    return (double)t->tv_sec + (double)t->tv_nsec * 1E-9;
}

static size_t _write_and_read_pipes(int write_fd, int read_fd, const void* write_buf,
                                    void* read_buf, size_t buf_nbytes) {

    assert(write_fd > 0 && read_fd > 0);
    assert(write_buf && read_buf);
    assert(buf_nbytes > 0);

    ssize_t nbytes_written = write(write_fd, write_buf, buf_nbytes);
    assert(nbytes_written > 0);
    ssize_t nbytes_read = read(read_fd, read_buf, (size_t)nbytes_written);
    assert(nbytes_read == nbytes_written);

    return (size_t)nbytes_written;
}

void _timed_read_and_write(int write_fd, int read_fd, double* seconds_passed, size_t* nbytes) {
    assert(seconds_passed && nbytes);

    static char *write_buf = NULL, *read_buf = NULL;

    if (!write_buf) {
        write_buf = calloc(1, _block_nbytes);
    }
    if (!read_buf) {
        read_buf = calloc(1, _block_nbytes);
    }

    struct timespec t0 = {0}, t1 = {0};

    _gettime(&t0);
    *nbytes = _write_and_read_pipes(write_fd, read_fd, write_buf, read_buf, _block_nbytes);
    _gettime(&t1);

    struct timespec delta_t = {0};

    timespecsub(&t1, &t0, &delta_t);

    *seconds_passed = _timespec_to_seconds(&delta_t);
}

static int _print_double(double x) {
    enum { BUF_NBYTES = 64 };
    char buf[BUF_NBYTES] = {0};
    snprintf(buf, BUF_NBYTES, "%f\n", x);
    write(STDOUT_FILENO, buf, strlen(buf));
}

int pipe_test_main(int argc, char** argv) {

    int pipe_fd[2] = {-1, -1};

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return -1;
    }

    double seconds_passed = 0.0;
    size_t nbytes = 0;

    double* times_ms = calloc(NSAMPLES, sizeof(double));

    for (size_t idx = 0; idx < WARMUP_ITERATIONS; ++idx) {
        _timed_read_and_write(pipe_fd[1], pipe_fd[0], &seconds_passed, &nbytes);
    }

    for (size_t idx = 0; idx < NSAMPLES; ++idx) {
        _timed_read_and_write(pipe_fd[1], pipe_fd[0], &seconds_passed, &nbytes);

        double blocks_transferred = (double)(nbytes) / (double)(_block_nbytes);

        times_ms[idx] = (seconds_passed / blocks_transferred) * 1000.;
    }

    for (size_t idx = 0; idx < NSAMPLES; ++idx) {
        _print_double(times_ms[idx]);
    }

    free(times_ms);

    return 0;
}
