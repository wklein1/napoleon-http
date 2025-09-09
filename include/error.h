#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>

#define EXIT_ON_ERROR(call, limit, msg) \
    do { \
        if ((call) < (limit)) { \
            perror(msg); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define LOG_ON_ERROR(call, limit, msg) \
    do { \
        if ((call) < (limit)) { \
            perror(msg); \
        } \
    } while (0)

#endif // ERROR_H
