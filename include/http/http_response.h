#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include "http_common.h"

/**
 * @brief Response description and helpers for writing a serialized response.
 *
 * The http core constructs a @ref http_response and passes it to
 * @ref http_send_response to serialize it to a file descriptor (e.g., socket).
 * This header also provides a convenience helper to send plain text.
 */

/**
 * @enum http_status
 * @brief Common status codes.
 *
 * These numeric codes are serialized into the status line by the writer.
 */
enum http_status{ 
	HTTP_OK 				= 200,
	HTTP_CREATED			= 201,
	HTTP_NO_CONTENT 		= 204,
	HTTP_BAD_REQUEST		= 400,
	HTTP_FORBIDDEN  		= 403,
    HTTP_NOT_FOUND  		= 404,
	HTTP_UNSUPPORTED		= 415,
	HTTP_SERVER_ERROR		= 500,
	HTTP_NOT_IMPLEMENTED	= 501
};


/**
 * @struct http_response
 * @brief Describes a response to be serialized.
 *
 * @note Ownership/lifetime:
 *  - @ref content_type must point to a NUL-terminated string valid until
 *    serialization completes; it may be NULL to select a default.
 *  - @ref extra_headers may be NULL or point to an array of @ref http_header
 *    of length @ref extra_headers_count; each name/value must be NUL-terminated.
 *  - @ref body may be NULL or point to a buffer of length @ref content_length.
 *  - @ref http_response_clear() will free @ref extra_headers and @ref body
 *    The struct itself is never freed by @ref http_response_clear().
 */
struct http_response {
    enum http_status status;      /**< Status code (e.g., 200, 404). */
    const char *content_type;     /**< MIME type string, or NULL for a default. */
    struct http_header *extra_headers; /**< Optional array of extra headers (may be NULL). */
    size_t extra_headers_count;   /**< Number of entries in @ref extra_headers. */
    const void *body;             /**< Optional response body buffer (may be NULL). */
    size_t content_length;        /**< Length of @ref body in bytes (0 if none). */
};

/**
 * @brief Send a HTTP response over a socket file descriptor.
 *
 * Writes an HTTP response including status line, headers and body.
 *
 * @param fd                  Socket file descriptor.
 * @param http_response       Http response struct to serialize (must not be NULL).
 *
 * @return 0 on success, -1 on error.
 */
int http_send_response(int fd, const struct http_response*);

/**
 * @brief Send a plain text response over a socket file descriptor.
 *
 * Wraps send_response() to simplify sending plain text responses 
 * without manually handling the body length.
 *
 * @param fd     Socket file descriptor.
 * @param status HTTP status code (e.g. 200, 404).
 * @param texr   Text string to send as body.
 *
 * @return 0 on success, -1 on error.
 */
int http_send_text(int fd, enum http_status status, const char *text);


/**
 * @brief Clear a response struct and free owned dynamic members.
 *
 * Frees @ref http_response::extra_headers (array) and @ref http_response::body, 
 * then resets all fields to zero/NULL.
 * The @ref http_response object itself is not freed.
 *
 * @param res Pointer to the response to clear (must not be NULL).
 *
 * @warning Only call this if @ref extra_headers and/or @ref body point to
 *          memory owned by the response (heap-allocated). Do **not** call it
 *          when these pointers refer to string literals or foreign storage.
 */
void http_response_clear(struct http_response *res);

#endif /* HTTP_RESPONSE_H  */
