#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif // _POSIX_C_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*pMainFn)(int, char**);

struct NamedTest {
    const char* name;
    pMainFn test_main;
};

/*
 * To add another test:
 *
 *  1. Declare the main symbol
 *  2. Create an entry in _NAMED_TESTS with a pointer to the main symbol.
 *
 * ...that's it!
 */

int phold_main(int, char**);
int pipe_test_main(int, char**);
int sleep_loops_main(int, char**);

static const struct NamedTest _NAMED_TESTS[] = {
    {.name = "PHOLD", .test_main = &phold_main},
    {.name = "SLEEP_LOOPS", .test_main = &sleep_loops_main},
    {.name = "PIPE_TEST", .test_main = &pipe_test_main},
};

enum { N_NAMED_TESTS = sizeof(_NAMED_TESTS) / sizeof(struct NamedTest) };

/*
 * Returns the NamedTest corresponding to the argument value if a name matches.
 * Otherwise, returns NULL.
 */
static const struct NamedTest* _get_test_value(const char* arg) {

    const struct NamedTest* retval = NULL;

    for (size_t idx = 0; arg && idx < N_NAMED_TESTS; ++idx) {
        if (strcmp(arg, _NAMED_TESTS[idx].name) == 0) {
            retval = &_NAMED_TESTS[idx];
        }
    }

    return retval;
}

static void _print_error() {

    char* buf = NULL;
    size_t buf_nbytes = 0;
    FILE* p_file = open_memstream(&buf, &buf_nbytes);
    assert(p_file);

    fprintf(p_file,
            "Specify a valid test to run using the first argument. Possible values:\n");

    for (size_t idx = 0; idx < N_NAMED_TESTS; ++idx) {
        fprintf(p_file, "%s\n", _NAMED_TESTS[idx].name);
    }

    fflush(p_file);
    fclose(p_file);

    if (buf) {
        fprintf(stderr, "%s", buf);
        free(buf);
    }
}

/*
 * Strips the test name argument from the command line arguments and passes
 * the rest to the test.
 */
static int _call_main_without_test_arg(pMainFn p_main, int argc, char **argv) {
    assert(p_main && argc >= 2 && argv);

    int retval = -1;
    char **argv_new = calloc(argc - 1, sizeof(char *));
    assert(argv_new);

    argv_new[0] = argv[0];

    for (int idx = 2; idx < argc; ++idx) {
      argv_new[idx - 1] = argv[idx];
    }

    retval = p_main(argc - 1, argv_new);

    free(argv_new);

    return retval;
}

int main(int argc, char** argv) {

    bool parsed_test = false;
    int retval = -1;

    if (argc > 1) {
        const struct NamedTest* named_test = _get_test_value(argv[1]);
        if (named_test) {
            parsed_test = true;
            assert(named_test->test_main);
            retval = _call_main_without_test_arg(named_test->test_main, argc, argv);
        }
    }

    if (!parsed_test) {
        _print_error();
    }

    return retval;
}
