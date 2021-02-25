#ifndef SNEAKY_TIME_H_
#define SNEAKY_TIME_H_

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

int sneaky_gettime(clockid_t clock_id, struct timespec *tp);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // SNEAKY_TIME_H_
