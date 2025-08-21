#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <stddef.h>
#include <stdint.h>

/**
 * @file http_request.h
 * @brief Data structures and helpers for representing a parsed HTTP request.
 *
 * This header defines fixed-size limits, the header and request structures,
 * and lifecycle helpers to initialize and free an http_request.
 */


/**
 * @def HTTP_MAX_METHOD
 * @brief Maximum number of bytes for the request method string (excluding the null terminator).
 */
#define HTTP_MAX_METHOD 		16

/**
 * @def HTTP_MAX_PATH
 * @brief Maximum number of bytes for the request path string (excluding the null terminator).
 */
#define HTTP_MAX_PATH 			2048

/**
 * @def HTTP_MAX_VERSION
 * @brief Maximum number of bytes for the HTTP version string (excluding the null terminator).
 */
#define HTTP_MAX_VERSION 		16

/**
 * @def HTTP_MAX_HEADERS
 * @brief Maximum number of header fields stored in a single request.
 */
#define HTTP_MAX_HEADERS 		32

/**
 * @def HTTP_MAX_HEADER_VALUE
 * @brief (Optional/Advisory) Maximum number of bytes for a single header value
 * (excluding the null terminator). Enforcement depends on the parser.
 */
#define HTTP_MAX_HEADER_VALUE 	50

/**
 * @def HTTP_MAX_HEADERS_BUFFER
 * @brief Upper bound for the size (in bytes) of the header section buffer when reading a request.
 */
#define HTTP_MAX_HEADERS_BUFFER 4096

/**
 * @def HTTP_MAX_BODY_BUFFER
 * @brief Upper bound for the size (in bytes) of the request body to be read/stored.
 */
#define HTTP_MAX_BODY_BUFFER 	4096


/**
 * @brief HTTP header struct containing Name/value pair for a single HTTP header field.
 *
 * Both @ref name and @ref value are heap-allocated, null-terminated
 * strings managed by the parser. They are freed by @ref http_request_free().
 */
struct http_header {
    char *name;   /**< Header field name (e.g., "Content-Type"), null-terminated. */
    char *value;  /**< Header field value (e.g., "text/plain"), null-terminated. */
};


/**
 * @brief HTTP request struct for representation of a parsed HTTP request.
 *
 * Fields @ref method, @ref path, @ref version, and each header name/value are
 * heap-allocated, null-terminated strings. They are freed by @ref http_request_free().
 *
 * The @ref headers array is also heap-allocated (size: @ref num_headers).
 * The @ref body pointer is an optional heap-allocated, null-terminated copy of
 * the message body (present only if a body was read/parsed).
 */
struct http_request {
    char *method;   /**< Request method (e.g., "GET", "POST"), null-terminated. */
    char *path;     /**< Request target/path (e.g., "/index.html"), null-terminated. */
    char *version;  /**< HTTP version (e.g., "HTTP/1.1"), null-terminated. */

    struct http_header *headers; /**< Array of headers (length: @ref num_headers). */
    size_t num_headers;          /**< Number of elements in @ref headers. */

    size_t content_length; /**< Parsed Content-Length value (if present), else 0. */
    char  *body;           /**< Optional body buffer, null-terminated; may be NULL if no body. */
};


/**
 * @brief Initialize an @ref http_request struct to a safe empty state.
 *
 * Sets all pointers to NULL and counters to zero so that @ref http_request_free()
 * can be safely called later even if parsing fails midway.
 *
 * @param req Pointer to the request object to initialize (must not be NULL).
 */
void http_request_init(struct http_request *req);


/**
 * @brief Free all heap-allocated memory in an @ref http_request struct.
 *
 * Frees @ref method, @ref path, @ref version, each header name/value, the
 * @ref headers array itself, and @ref body if present. After this call, the
 * structure is reset to an empty state (NULL pointers, zero counters).
 * 
 * Does not free the struct itself!
 * If the req struct is heap allocated itself, it has to be freed by the owner.
 *
 * @param req Pointer to the http_request struct to free (must not be NULL).
 */
void http_request_free(struct http_request *req);

#endif /* HTTP_REQUEST_H */
