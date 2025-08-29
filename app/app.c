#include "../include/app.h"
#include <stdio.h>
#include <stdlib.h>

int app_handle_client(const struct app_request *req, struct app_response *res){
    if(!req || !res) return -1;
	char *body = calloc(13, sizeof(char));
	if(!body) return -1;
	snprintf(body, 13, "Hello World\n");
	
	res->status = APP_OK;
	res->payload = body;
	res->payload_len = 12;
	res->media = APP_MEDIA_TEXT;
    return 0;
}
