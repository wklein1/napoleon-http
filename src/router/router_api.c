#include "../../include/router/router_api.h"
#include <stdio.h>
#include <string.h>

void api_router_init(struct api_router *router, const char *prefix,
                     struct api_route *route_table, size_t routes_cap){

    router->prefix      = prefix ? prefix : "/api";
    router->routes      = route_table;
    router->route_count = 0;
    router->max_routes  = routes_cap;
}


int api_router_add(struct api_router *router, enum app_method method,
                   		const char *path, api_route_handler handler){

	if (!router || !path || !handler || router->route_count >= router->max_routes) return -1;
	
	struct api_route route = {
        .method  = method,
        .path    = path,
        .handler = handler,
    };

    router->routes[router->route_count++] = route;
    return 0;
}


int api_router_handle(struct api_router *router, const struct app_request *req, struct app_response *out){

	size_t prefix_len = strlen(router->prefix);
	 if (prefix_len > 0) {
        if (strncmp(req->path, router->prefix, prefix_len) != 0) {
            return 1;
        }

        char next_char = req->path[prefix_len];
        if (next_char != '\0' && next_char != '/') {
            return 1;
        }
    }

	size_t i=0;
	while(i < router->route_count){
		if(router->routes[i].method == req->method && strcmp(router->routes[i].path, req->path + prefix_len)==0){
			return router->routes[i].handler(req, out);
		}
		i++;
	}

	static const char message[] = "API route not found\n";
    out->status	  	   = APP_NOT_FOUND;
    out->media_type    = APP_MEDIA_TEXT;
    out->payload  	   = message;
    out->payload_len   = sizeof(message) - 1;
	out->payload_owned = false;
	return 0;
	
}
