#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif // _POSIX_C_SOURCE

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* _TEST_ENV_VAR = "SHADOW_BMARK";

typedef int (*pMainFn) (int, char**);

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

int phold_main(int, char **);
int sleep_loops_main(int, char **);

static const struct NamedTest _NAMED_TESTS[] = {
    {.name = "PHOLD", .test_main = &phold_main},
    {.name = "SLEEP_LOOPS", .test_main = &sleep_loops_main},
};

enum { N_NAMED_TESTS = sizeof(_NAMED_TESTS) / sizeof(struct NamedTest) };

/*
 * Returns the NamedTest corresponding to the value in SHADOW_BMARK if the test
 * name matches. Otherwise, returns NULL.
 */
static const struct NamedTest* _get_env_test_value() {

    struct NamedTest *retval = NULL;

    const char* test_value = getenv(_TEST_ENV_VAR);

    for (size_t idx = 0; test_value && idx < N_NAMED_TESTS; ++idx) {
        if (strcmp(test_value, _NAMED_TESTS[idx].name) == 0) {
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
            "Specify a valid test to run using the %s environment variable. Possible values:\n",
            _TEST_ENV_VAR);

    for (size_t idx = 0; idx < N_NAMED_TESTS; ++idx) {
      fprintf(p_file, "%s=%s\n", _TEST_ENV_VAR, _NAMED_TESTS[idx].name);
    }

    fflush(p_file);
    fclose(p_file);

    if (buf) {
      fprintf(stderr, "%s", buf);
      free(buf);
    }
}

int main(int argc, char** argv) {

    int retval = 0;

    const struct NamedTest* named_test = _get_env_test_value();

    if (named_test) {
      assert(named_test->test_main);
      retval = named_test->test_main(argc, argv);
    } else {
      _print_error();
      retval = -1;
    }

    return retval;
}
