/**
 * @file util.h
 *
 * @brief
 *
 * @date 3/31/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_UTIL_H
#define ARES_UTIL_H

#if __cplusplus
extern "C" {
#endif

#define STRINGIFY(x)       #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)
#define ARG_UNUSED(x)      (void)x

#if __cplusplus
}
#endif

#endif // ARES_UTIL_H
