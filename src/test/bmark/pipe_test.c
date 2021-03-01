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

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

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

_Static_assert(sizeof(clockid_t) >= 4);
enum { PASSTHRU_CLOCK_MONOTONIC = 66666 };

static bool _use_passthru_clock = false;
int asm_gettime(struct timespec* t);

static inline int _gettime(struct timespec* tp) {
    if (_use_passthru_clock) {
        return clock_gettime(PASSTHRU_CLOCK_MONOTONIC, tp);
    } else {
        return asm_gettime(tp);
    }
}

static size_t _block_nbytes = 1 << 15;
static const char* _PORT = "45425";

enum { WARMUP_ITERATIONS = 1000, NSAMPLES = 10000 };

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
    snprintf(buf, BUF_NBYTES, "%.9f\n", x);
    write(STDOUT_FILENO, buf, strlen(buf));
}

static int _setup_server_fd() {
    struct addrinfo hints = {0}, *res = NULL;
    int sockfd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(NULL, _PORT, &hints, &res);

    if (rc) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // make a socket:

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // bind it to the port we passed in to getaddrinfo():

    rc = bind(sockfd, res->ai_addr, res->ai_addrlen);

    if (rc) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

static int _setup_client_fd() {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int rc = getaddrinfo("127.0.0.1", _PORT, &hints, &res);

    if (rc) {
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    rc = connect(sockfd, res->ai_addr, res->ai_addrlen);

    if (rc) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

int pipe_test_main(int argc, char** argv) {

    const char* usage = "Usage: %s 0|1\nPass 1 if running inside Phantom, otherwise 0.\n";

    if (argc < 2) {
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "0") == 0) {
        _use_passthru_clock = false;
    } else if (strcmp(argv[1], "1") == 0) {
        _use_passthru_clock = true;
    } else {
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    int pipe_fd[2] = {-1, -1};

    pipe_fd[0] = _setup_server_fd();
    pipe_fd[1] = _setup_client_fd();

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
