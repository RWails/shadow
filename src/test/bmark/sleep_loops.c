#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif // _POSIX_C_SOURCE

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int asm_gettime(struct timespec* t);

static struct timespec CTOR_TIME = {0};
static struct timespec SLEEP_TM = {0, 10}; // 10 ns

__attribute__((constructor)) static void _sample_time() { asm_gettime(&CTOR_TIME); }

static inline double _timespec_to_seconds(const struct timespec* t) {
    return (double)t->tv_sec + (double)t->tv_nsec * 1E-9;
}

static inline double _duration_to_seconds(const struct timespec* t0, const struct timespec* t1) {
    return _timespec_to_seconds(t1) - _timespec_to_seconds(t0);
}

static const char* USAGE_PATTERN = "USAGE: %s n_workitrs n_loops\n\
    \n\
    n_workitrs >= 0 is the number of sine computations to perform per loop.\n\
    n_loops >= 0 is the number of sleep-and-sine loops to run.\n\
";

static int _parse_args(char** argv, size_t* n_workitrs, size_t* n_loops) {

    int rc = sscanf(argv[1], "%zu", n_workitrs);
    if (rc != 1) {
        fprintf(stderr, "Error parsing %s as n_workitrs.\n", argv[1]);
        return rc;
    }
    rc = sscanf(argv[2], "%zu", n_loops);
    if (rc != 1) {
        fprintf(stderr, "Error parsing %s as n_loops.\n", argv[2]);
        return rc;
    }

    return 0;
}

/*
 * A bunch of helper print functions that perform I/O directly through calls to
 * write. These helpers allow the syscalls to be intercepted in Shadow's
 * preload mode without requiring a custom, preloaded libc.
 */
static void _print_string(int fd, const char* str) { write(fd, str, strlen(str)); }

static void _print_args(int fd, size_t n_workitrs, size_t n_loops) {
    enum { kBufNBytes = 512 };
    char buf[kBufNBytes] = {0};
    snprintf(buf, kBufNBytes, "n_workitrs: %zu\nn_loops: %zu\n", n_workitrs, n_loops);
    _print_string(fd, buf);
}

static void _print_double(int fd, double x) {
    enum { kBufNBytes = 512 };
    char buf[kBufNBytes] = {0};
    snprintf(buf, kBufNBytes, "%f\n", x);
    _print_string(fd, buf);
}

static void _print_size_t(int fd, size_t x) {
    enum { kBufNBytes = 512 };
    char buf[kBufNBytes] = {0};
    snprintf(buf, kBufNBytes, "%zu\n", x);
    _print_string(fd, buf);
}

static void _print_tm(int fd, const struct tm* info) { _print_string(fd, asctime(info)); }

static void _print_current_time(int fd) {
    time_t now = {0};
    struct tm* info = NULL;
    time(&now);
    info = localtime(&now);
    _print_tm(fd, info);
}

static void _print_timespec(int fd, const struct timespec* t) {
    struct tm* info = localtime(&t->tv_sec);
    _print_tm(fd, info);
    _print_double(fd, _timespec_to_seconds(t));
}

static inline void _work(size_t n_workitrs, size_t i) {

    static volatile float x = 0; // helper to prevent loop optimization.

    float i_f = (float)i, acc = 0.0f;

    for (size_t idx = 0; idx < n_workitrs; ++idx) {
        float idx_f = (float)idx;
        acc += sin(idx_f + i_f);
        __asm__ volatile("");
    }

    x += acc;
}

int sleep_loops_main(int argc, char** argv) {

    struct timespec start_time = {0}, rstart_time = {0}, rend_time = {0};
    asm_gettime(&start_time);

    size_t n_workitrs = 0, n_loops = 0;

    if (argc != 3 || _parse_args(argv, &n_workitrs, &n_loops)) {
        fprintf(stderr, USAGE_PATTERN, argv[0]);
        return -1;
    }

    _print_string(STDOUT_FILENO, "Init time:\t");
    _print_timespec(STDOUT_FILENO, &CTOR_TIME);
    _print_string(STDOUT_FILENO, "Plugin start time:\t");
    _print_timespec(STDOUT_FILENO, &start_time);
    _print_string(STDOUT_FILENO, "^ Startup time:\t");
    _print_double(STDOUT_FILENO, _duration_to_seconds(&CTOR_TIME, &start_time));

    _print_args(STDOUT_FILENO, n_workitrs, n_loops);

    double ewma = 0.0;

    for (size_t idx = 0; idx < n_loops; ++idx) {
        asm_gettime(&rstart_time);
        _work(n_workitrs, idx);
        nanosleep(&SLEEP_TM, NULL);
        asm_gettime(&rend_time);

        double round_time = _duration_to_seconds(&rstart_time, &rend_time);
        double cumul_round_time = _duration_to_seconds(&start_time, &rend_time);

        ewma = (0.75 * ewma) + (0.25 * round_time);

        _print_size_t(STDOUT_FILENO, idx);
        _print_string(STDOUT_FILENO, "$ Round time:\t");
        _print_double(STDOUT_FILENO, round_time);
        _print_string(STDOUT_FILENO, "# Weighted moving average round time:\t");
        _print_double(STDOUT_FILENO, ewma);
        _print_string(STDOUT_FILENO, "@ Cumul round time:\t");
        _print_double(STDOUT_FILENO, cumul_round_time);
    }

    return 0;
}
