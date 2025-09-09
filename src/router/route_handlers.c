#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/router/route_handlers.h"
#include "../../include/app.h"

int handle_route_echo(const struct app_request *req, struct app_response *res){
	if(req->payload && req->payload_len > 0){
		res->payload_len = req->payload_len;
		res->payload 	 = req->payload;
		res->media_type  = req->media_type;
		res->payload_owned = false;
	}else{
		const char *method_name = NULL;

		switch(req->method){
			case APP_GET: {
				method_name = "GET";
				break;
			}
			case APP_POST: {
				method_name = "POST";
				break;
			}
			default: {
				res->media_type = APP_MEDIA_NONE;
				res->payload = NULL;
				res->payload_len = 0; 
				res->payload_owned = false;
				res->status = APP_BAD_REQUEST;
				return 0;
			};
		}

		size_t req_line_size = strlen(method_name) + strlen(req->path)+2;
		char *res_line_buffer = calloc(req_line_size,sizeof(char));
		if(!res_line_buffer) return -1;
    	int written = snprintf(res_line_buffer, req_line_size, "%s %s", method_name, req->path);
		if(written < 0) return -1;
			res->media_type  = APP_MEDIA_TEXT;
			res->payload     = res_line_buffer;
			res->payload_len = req_line_size-1;
			res->payload_owned = true;
	}
		res->status = APP_OK;
		return 0;
}
