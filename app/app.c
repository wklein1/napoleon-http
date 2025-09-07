#include "../include/app.h"
#include "../include/router/router_api.h"
#include "../include/router/router_static.h"
#include "../include/router/route_handlers.h"
#include <stdbool.h>

static struct api_router api_router;
static struct static_router static_router;
static bool app_inited = false;

int app_init(struct fs *vfs) {
    if (app_inited) return 0;
	if(!vfs) return -1;
    static struct api_route route_table[MAX_ROUTES]; 
    api_router_init(&api_router, "/api", route_table, MAX_ROUTES);
    if(api_router_add(&api_router, APP_GET,  "/echo", handle_route_echo)<0) return -1;
    if(api_router_add(&api_router, APP_POST, "/echo", handle_route_echo)<0) return -1;

    static_router_init(&static_router, "/public", vfs, "index.html", 500 * 1024);

	app_inited = true;
    return 0;
}

int app_handle_client(const struct app_request *req, struct app_response *res){    
    if(!req || !res) return -1;
	if(!app_inited)  return -1;

	int	api_router_res = api_router_handle(&api_router, req, res);
	if(api_router_res < 0) return -1;
	int static_router_res = static_router_handle(&static_router, req, res);
	if(static_router_res < 0) return -1;

	if(api_router_res==1 && static_router_res ==1){
		static const char message[] = "Route not found\n";
    	res->status        = APP_NOT_FOUND;
    	res->media_type    = APP_MEDIA_TEXT;
    	res->payload       = message;
    	res->payload_len   = sizeof(message) - 1;
		res->payload_owned = false;
		return 0;
	}
	return 0;
}
