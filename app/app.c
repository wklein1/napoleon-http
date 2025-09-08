#include <stdbool.h>
#include "../include/app.h"
#include "../include/router/router_api.h"
#include "../include/router/router_static.h"
#include "../include/router/route_handlers.h"
#include "../include/router/redirect_registry.h"


static struct api_router 		api_router;

static struct static_router 	static_routers[MAX_STATIC_ROUTERS];
static size_t					static_router_count = 0;

static struct redirect_registry redirects;
static struct redirect_rule		redirect_rules[MAX_REDIRECTS];

static bool app_inited = false;


int app_init(const struct app_mount *mounts, size_t mount_count){

    if (app_inited) return 0;
    if (mount_count > 0 && !mounts) return -1;
    if (mount_count > MAX_STATIC_ROUTERS) return -1;

    /* #### API ROUTES #### */
    static struct api_route route_table[MAX_ROUTES];
    api_router_init(&api_router, "/api", route_table, MAX_ROUTES);
    if(api_router_add(&api_router, APP_GET,  "/echo", handle_route_echo)<0) return -1;
    if(api_router_add(&api_router, APP_POST, "/echo", handle_route_echo)<0) return -1;


    /* #### STATIC ROUTERS #### */
    for (size_t i = 0; i < mount_count; i++) {
		if (!mounts[i].vfs || !mounts[i].prefix) return -1;
        static_router_init(&static_routers[i], mounts[i].prefix, mounts[i].vfs,
						   mounts[i].index_name, mounts[i].max_bytes);
    }
    static_router_count = mount_count;


    /* #### REDIRECTS #### */
    redirect_registry_init(&redirects, redirect_rules, false, MAX_REDIRECTS, 0);
    if(redirect_add(&redirects, "/",		"/docs/", 	EXACT, false, APP_REDIRECT_PERMANENT)<0) return -1;
    if(redirect_add(&redirects, "/docs", 	"/docs/", 	EXACT, false, APP_REDIRECT_PERMANENT)<0) return -1;
    if(redirect_add(&redirects, "/public", "/public/",  EXACT, false, APP_REDIRECT_PERMANENT)<0) return -1;

    app_inited = true;
    return 0;
}

int app_make_redirect(struct app_response *res, const char *location,
                      bool location_owned, enum app_redirect_type type){

    if(!res || !location) return -1;

    res->status			= APP_OK;
    res->media_type		= APP_MEDIA_NONE;
    res->payload		= NULL;
    res->payload_len	= 0;
    res->payload_owned	= false;

    res->redirect.enabled		 = true;
    res->redirect.location		 = location;
    res->redirect.location_owned = location_owned;
    res->redirect.type			 = type;
    return 0;
}


int app_handle_client(const struct app_request *req, struct app_response *res){

    if(!req || !res) return -1;
	if(!app_inited)  return -1;

	struct redirect_result redirect_res = {0};
	int redirect_ret = redirect_lookup(&redirects, req->path, &redirect_res);
	if (redirect_ret == 0){
    	return app_make_redirect(res, redirect_res.target, redirect_res.target_owned, redirect_res.type);
	}
	if (redirect_ret < 0) return -1;

	int api_router_res = api_router_handle(&api_router, req, res);
	if(api_router_res == 0) return  0;
    if(api_router_res < 0)  return -1;

	for (size_t i = 0; i < static_router_count; i++) {
        int static_router_res = static_router_handle(&static_routers[i], req, res);
        if(static_router_res == 0) return  0;
        if(static_router_res < 0)  return -1;
    }

	static const char message[] = "Route not found\n";
    res->status        = APP_NOT_FOUND;
    res->media_type    = APP_MEDIA_TEXT;
    res->payload       = message;
    res->payload_len   = sizeof(message) - 1;
	res->payload_owned = false;
	res->redirect.enabled = false;
	return 0;
}
