#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../../include/http/http_response.h"

/**
 * @brief Write the entire buffer to a file descriptor, handling partial writes.
 *
 * This function repeatedly calls @c write(2) until either all requested
 * bytes from @p buffer have been written to the file descriptor @p fd,
 * or an unrecoverable error occurs.
 *
 * - If @c write is interrupted by a signal (@c EINTR), the call is retried.
 * - On any other error, the function stops and returns -1.
 *
 * @param fd
 *        File descriptor to write to (e.g. a socket or pipe).
 * @param buffer
 *        Pointer to the data buffer to write.
 * @param buffer_len
 *        Number of bytes to write from @p buffer.
 *
 * @return
 *        Total number of bytes written on success,
 *        or -1 on error (errno is preserved).
 */
static ssize_t write_all(int fd, const void *buffer, size_t buffer_len){
    size_t total_written = 0;
	ssize_t currently_written = 0;
	while(total_written<buffer_len){
		
		currently_written = write(fd, (const char*)buffer + total_written, 
							buffer_len - total_written);
		if(currently_written < 0){
			if(errno == EINTR) continue;
			return -1;
		}
		if(currently_written == 0){
			return -1;
		}
		total_written += currently_written;
	}
	return total_written;
}


/**
 * @brief Translate an HTTP status code to its reason phrase.
 *
 * Provides the textual reason phrase associated with an HTTP status code,
 * as used in the status line of an HTTP response.
 *
 * Currently only a small subset of common status codes are supported.
 * Unknown codes return the generic phrase @c "Not Implemented".
 *
 * @param status
 *        A status code represented by a value of the http_status enum 
 *        (e.g. HTTP_OK, HTTP_NOT_FOUND).
 *
 * @return
 *        A constant string with the reason phrase (e.g. "OK", "Not Found").
 *        The returned string is statically allocated and must not be freed.
 */
static const char* get_reason_phrase(enum http_status status){
	switch (status) {
		case HTTP_OK: return "OK";
		case HTTP_CREATED: return "Created";
		case HTTP_NO_CONTENT: return "No Content";
		case HTTP_BAD_REQUEST: return "Bad Request";
		case HTTP_FORBIDDEN: return "Forbidden";
		case HTTP_NOT_FOUND: return "Not Found";
		case HTTP_UNSUPPORTED: return "Unsupported Media Type";
		case HTTP_SERVER_ERROR: return "Internal Server Error";
		case HTTP_NOT_IMPLEMENTED: return "Not Implemented";
		default: return "Not Implemented";
	}
}

void http_response_clear(struct http_response *res){
	res->status=HTTP_OK;
	res->content_length=0;
	res->extra_headers_count=0;
	res->content_type=NULL;
	if(res->extra_headers) free(res->extra_headers);
	if(res->body) free((void*)res->body);
}

int http_send_response(int fd, const struct http_response *res){  

	if (!res) { errno = EINVAL; return -1; }
    const char *content_type = res->content_type ? res->content_type : "text/plain; charset=utf-8";
	
	char headers[2048];

	char end_of_headers[] = "Connection: close\r\n\r\n"; 

	size_t headers_limit = sizeof(headers)-strlen(end_of_headers);
    
	size_t h_written = 0;
	size_t cap = headers_limit - h_written;
	int currently_written = snprintf(headers+h_written, cap,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n",
        res->status, get_reason_phrase(res->status), content_type, res->content_length);

	if (currently_written < 0 || (size_t)currently_written >= cap) return -1;
	h_written += (size_t)currently_written;

	for(size_t i=0; i<res->extra_headers_count; i++){
		int to_write = snprintf(NULL, 0, "%s: %s\r\n",
			res->extra_headers[i].name, res->extra_headers[i].value);
        if (to_write < 0) return -1;

        if (h_written + (size_t)to_write + strlen(end_of_headers) > sizeof(headers)){
            break;
        }

		int currently_written = snprintf(headers+h_written, cap, "%s: %s\r\n",
		res->extra_headers[i].name, res->extra_headers[i].value);

		if(currently_written < 0 || (size_t)currently_written >= cap) return -1;
		h_written += (size_t)currently_written;
	}
	currently_written = 0;
	currently_written = snprintf(headers + h_written, sizeof(headers) - h_written, "%s", end_of_headers);
    if (currently_written < 0) return -1;
	h_written+=currently_written;

	int headers_written_count = write_all(fd, headers, h_written);
	if (headers_written_count < 0) return -1;

	if(res->body && res->content_length > 0){
		int body_written_count = write_all(fd, res->body, res->content_length);
		if (body_written_count < 0) return -1;
	}

	return 0;
}


int http_send_text(int fd, enum http_status status, const char *text){
    if (!text) { errno = EINVAL; return -1; }

    struct http_response res = {
        .status = status,
        .content_type = "text/plain; charset=utf-8",
        .content_length = strlen(text),
        .body = text,
        .extra_headers_count = 0,
    };

    return http_send_response(fd, &res);
}
