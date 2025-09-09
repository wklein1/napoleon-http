#include "../../include/http/http_request.h"
#include <stdio.h>
#include <stdlib.h> 
#include <strings.h>

void http_request_init(struct http_request *req) {
    if (!req) return;
	req->method=NULL;
	req->path=NULL;
	req->version=NULL;
	req->headers=NULL;
    req->num_headers = 0;
    req->content_length = 0;
    req->body = NULL;
}

void http_request_clear(struct http_request *req) {
    if (!req) return;
	free(req->method);
	free(req->path);
	free(req->version);
	if(req->headers){
		for(size_t i=0;i<req->num_headers;i++){
			if (req->headers[i].name && req->headers[i].name_owned == true) free((void*)req->headers[i].name);
			if (req->headers[i].value && req->headers[i].value_owned == true) free((void*)req->headers[i].value);
		}
		free(req->headers);
    }
	if(req->body)free(req->body);
	http_request_init(req);
}

const char* http_request_get_header_value(const struct http_request *req, const char *header_name){
	for(size_t i=0; i<req->num_headers; i++){
		if(strcasecmp(req->headers[i].name, header_name) == 0){
			return req->headers[i].value;
		}
	}
	return NULL;
}
