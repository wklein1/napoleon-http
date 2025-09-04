#include "../../include/router/router_static.h"
#include <string.h>

static enum app_media media_from_ext(const char *ext) {

    if (!strcasecmp(ext, ".html") || !strcasecmp(ext, ".htm"))  return APP_MEDIA_HTML;
    if (!strcasecmp(ext, ".js"))								return APP_MEDIA_JS;
    if (!strcasecmp(ext, ".css"))								return APP_MEDIA_CSS;
    if (!strcasecmp(ext, ".txt"))								return APP_MEDIA_TEXT;
    if (!strcasecmp(ext, ".json"))								return APP_MEDIA_JSON;

    return APP_MEDIA_BIN;
}


static int build_rel_path(const char *path, const char *index_name, char **rel_path_out){
	return 0;
}

void static_router_init(struct static_router *router, const char *prefix, struct fs *vfs, 
						const char *index_name, size_t max_bytes){
	if(!router || !vfs) return;
	router->prefix = prefix ? prefix : "/public";
	router->vfs = vfs;
	router->index_name = index_name ? index_name : "index.html";
	router->max_bytes = max_bytes;
}


int static_router_handle(struct static_router *router, const struct app_request *req,
                         struct app_response *out){
	
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
	if (req->method != APP_GET) return 1; //TODO: 405 response
	
	char *rel_path = NULL;
    if (build_rel_path(req->path, router->index_name, &rel_path) < 0) {
        return -1;
    }
	return 0;
}
