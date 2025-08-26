#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../../include/http/http_response.h"
#include "../../include/http/http_mime.h"

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
static int write_all(int fd, const void *buffer, size_t buffer_len){
    size_t total_written = 0;
	ssize_t currently_written = 0;
	while(total_written<buffer_len){
		
		currently_written = write(fd, (const char*)buffer + total_written, 
							buffer_len - total_written);
		if(currently_written <0){
			if(errno == EINTR) continue;
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
 * Unknown codes return the string @c "NOT IMPLEMENTED".
 *
 * @param status
 *        The numeric HTTP status code (e.g. 200, 404).
 *
 * @return
 *        A constant string with the reason phrase (e.g. "OK", "Not Found").
 *        The returned string is statically allocated and must not be freed.
 */
static const char* get_reason_phrase(int status){
	switch (status) {
		case 200: return "OK";
		case 400: return "Bad Request";
		case 404: return "Not Found";
		case 500: return "Internal Server Error";
		default: return "NOT IMPLEMENTED";
	}
}



int send_response(int fd, int status, const char *content_type, 
				  const void *body, size_t body_len){

	if (!content_type) content_type = get_mime_type("txt");
    
	char headers[2048];
    
	int n = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, get_reason_phrase(status), content_type, body_len);

	if (n < 0) return -1;

	int headers_written_count = write_all(fd, headers, (size_t)n);
	if (headers_written_count < 0) return -1;

	if(body && body_len > 0){
		int body_written_count = write_all(fd, body, body_len);
		if (body_written_count < 0) return -1;
	}

	return 0;
}

int send_text(int fd, int status, const char *text){
	if (!text) return -1;
    return send_response(fd, status, get_mime_type("txt"),
                         text, strlen(text));
}
