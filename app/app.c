#include "../include/app.h"
#include "../include/http/http.h"
#include "../include/http/http_parser.h"
#include <stdlib.h>
#include <string.h>

int app_handle_client(int client_fd){
    const char *body = "Hello World\n";
	void * buff = calloc(1,250);
	struct http_request *req = calloc(1,sizeof(struct http_request));
	http_request_init(req);
	if(!req){
		return -1;
	}

	if(buff){
		http_parse_request(client_fd, &buff, 250, req);
		free(buff);
	}

	http_request_free(req);
	free(req);

    return send_response(client_fd, 200, "text/plain; charset=UTF-8",
                         body, strlen(body));
}
