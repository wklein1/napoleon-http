#include "../../include/http/http_request.h"
#include <stdio.h>
#include <stdlib.h> 

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

void http_request_free(struct http_request *req) {
    if (!req) return;
	free(req->method);
	free(req->path);
	free(req->version);
	for(size_t i=0;i<req->num_headers;i++){
		free(req->headers[i].name);
		free(req->headers[i].value);
	}
	free(req->headers);
	http_request_init(req);
}
