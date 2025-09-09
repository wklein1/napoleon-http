#include "../../include/core/http_core.h"
#include "../../include/http/http_parser.h"
#include "../../include/http/http_request.h"
#include <stdlib.h>


int http_handle_connection(int client_fd, void *context){
	void * buff = calloc(1,250);
	if(!buff){
		return -1;
	}
	struct http_request *req = calloc(1,sizeof(struct http_request));
	int ret = -1;
	if(!req) goto cleanup_buff;
	http_request_init(req);

	if(http_parse_request(client_fd, &buff, 250, req)<0) goto cleanup_req;

	struct http_core_ctx *http_core_context = (struct http_core_ctx*)context;
	struct http_response res = {0};
    ret = http_core_context->adapter_handler(req, &res, http_core_context->adapter_context);
	
	if(ret >= 0){
		http_send_response(client_fd, &res);
	}

	http_response_clear(&res);

	cleanup_req:
    	http_request_clear(req);
    	free(req);
	cleanup_buff:
    	free(buff);
    	return ret;
}
