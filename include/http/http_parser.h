#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>
#include "http_request.h"

/**
 * @brief Parse the HTTP request line ("METHOD PATH VERSION") into request struct.
 *
 * Allocates strings for method, path, and version in the request struct.
 *
 * @param buffer 		Input buffer containing the request line.
 * @param buffer_len 	Length of the buffer.
 * @param req 			HTTP request struct to populate.
 *
 * @return int 			Index of first character after the request line, or -1 on error.
 */
int http_parse_request_line(const char *buffer, size_t buffer_len, struct http_request *req);


/**
 * @brief Parse HTTP headers from buffer into request struct.
 *
 * Allocates and copies header names/values into req->headers.
 *
 * @param buffer 					Input buffer containing headers.
 * @param buffer_len 				Length of the buffer.
 * @param headers_dropped [out] 	Number of headers dropped due to max limit.
 * @param req 						HTTP request struct to populate.
 *
 * @return int Number of headers parsed, or -1 on error.
 */
int http_parse_request_headers(const char *buffer, size_t buffer_len, int *headers_dropped, struct http_request *req);


/**
 * @brief Read the HTTP request body based on Content-Length.
 *
 * Allocates string for body in the request struct
 *
 * @param fd 			File descriptor to read from.
 * @param buffer 		Pointer to buffer pointer (gets reallocated if needed).
 * @param buffer_len 	Current buffer length.
 * @param headers_end 	Index where headers end.
 * @param content_len 	Declared Content-Length.
 * @param max_body 		Maximum allowed body size.
 * @param already_read  Bytes already read into buffer.
 * @param req 			HTTP request struct to populate with body pointer.
 *
 * @return int 			Number of bytes read into body, or -1 on error.
 */
int read_body(int fd, char **buffer, size_t buffer_len, size_t headers_end, 
              size_t content_len, size_t max_body, size_t already_read, struct http_request *req);


/**
 * @brief Parse a complete request from @p fd into @p req.
 *
 * On success returns 0 and fills `req` (allocates fields if necessery).
 * On error returns -1. Caller must call http_request_clear(req). 
 *
 * @param fd 			File descriptor to read from.
 * @param buffer 		Pointer to buffer pointer (gets reallocated if needed).
 * @param buffer_len 	Initial buffer length.
 * @param req 			HTTP request struct to populate.
 *
 * @return int 			0 on success, -1 on error.
 */
int http_parse_request(int fd, void **buffer, size_t buffer_len, struct http_request *req);

#endif /* HTTP_PARSER_H */
