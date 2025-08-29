#ifndef HTTP_COMMON_H
#define HTTP_COMMON_H

/**
 * @file http_common.h
 * @brief Representation of a single HTTP header name/value pair.
 *
 * This struct is shared by request and response handling code. It does not
 * specify ownership semantics by itself: depending on context, `name` and
 * `value` may be:
 *  - heap-allocated (e.g., by the HTTP parser),
 *  - static string literals,
 *  - or provided by the application.
 *
 * Ownership rules are documented in the respective modules:
 *  - For requests: the parser allocates and frees them via
 *    @ref http_request_free().
 *  - For responses: the application/core decide whether strings are
 *    static or managed elsewhere.
 */
#include <stddef.h>
struct http_header {
    char *name;   /**< Header field name (e.g., "Content-Type"), null-terminated. */
    char *value;  /**< Header field value (e.g., "text/plain"), null-terminated. */
};


#endif /* HTTP_COMMON_H */
