#ifndef HTTP_H
#define HTTP_H
#include <stddef.h>


/**
 * @brief Struct containing a file extension and it's MIME type string.
 *
 * Represents a simple mapping of a file extension (e.g. "html", "json")
 * with the corresponding MIME type string as used in HTTP headers.
 */
struct mime_type {
    const char *ext; 	/**< File extension without leading dot (e.g. "html"). */
    const char *mime;	/**< MIME type string, including optional charset (e.g. "text/html; charset=UTF-8"). */
};

/**
 * @brief Send a HTTP response over a socket file descriptor.
 *
 * Writes an HTTP response including status line, headers and body.
 *
 * @param fd           Socket file descriptor.
 * @param status       HTTP status code (e.g. 200, 404).
 * @param content_type MIME type of the response body (e.g. "text/html").
 * @param body         Pointer to the response body data.
 * @param body_len     Length of the response body in bytes.
 *
 * @return 0 on success, -1 on error.
 */
int send_response(int fd, int status, const char *content_type,
                  const void *body, size_t body_len);

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
int send_text(int fd, int status, const char *text);

#endif /* HTTP_H  */
