/*
 * SeldOS - Humboldt Kernel Project
 * SNL C Standard Library (libsnl)
 * Minimal Math definitions
 */

#ifndef _MATH_H_
#define _MATH_H_

static inline double fabs(double x) {
    return x < 0.0 ? -x : x;
}

#endif /* _MATH_H_ */
