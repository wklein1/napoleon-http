#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include <stddef.h>

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

#endif /* HTTP_RESPONSE_H  */
