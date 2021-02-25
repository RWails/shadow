#include <stdlib.h>
#include <unistd.h>

/*
 * This definition is provided by Shadow and makes a real syscall. The
 * implementation here should *never* be used.
 */
void shadow_intercepted_gettime(struct timespec *t) {
    static const char *fail_str = "gettime function was not intercepted!\n";
    write(STDERR_FILENO, fail_str, strlen(fail_str));
    exit(EXIT_FAILURE);
    while (1) ;
}
