#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../../include/http/http.h"

static const struct mime_type mime_map[] = {
    { "txt",  "text/plain; charset=UTF-8" },
    { "html", "text/html; charset=UTF-8" },
    { "json", "application/json; charset=UTF-8" },
	{ "css",  "text/css; charset=UTF-8" },
	{ "js",   "application/javascript; charset=UTF-8" },
};

static int write_all(int fd, const void *buffer, size_t buffer_len){
    size_t total_written = 0;
	ssize_t currently_written = 0;
	while(total_written<buffer_len){
		
		currently_written = write(fd, buffer, buffer_len - currently_written);
		if(currently_written <0){
			if(errno == EINTR) continue;
			return -1;
		}
		total_written += currently_written;
	}
	return total_written;
}

static const char* get_status_message(int status){
	switch (status) {
		case 200: return "OK";
		case 400: return "Bad Request";
		case 404: return "Not Found";
		case 500: return "Internal Server Error";
		default: return "NOT IMPLEMENTED";
	}
}

static const char* get_mime_type(const char* extension){
	for (size_t i=0; i < (sizeof(mime_map)/sizeof(struct mime_type)); i++){
		if(strcmp(extension, mime_map[i].ext) == 0) return mime_map[i].mime;
	}
	return NULL;
};


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
        status, get_status_message(status), content_type, body_len);

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
