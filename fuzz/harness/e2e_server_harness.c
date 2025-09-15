#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../include/core/http_core.h"
#include "../../include/adapters/adapter_http_app.h"
#include "../../include/router/router_api.h"
#include "../../include/router/route_handlers.h"
#include "../../include/router/router_static.h"
#include "../../include/router/redirect_registry.h"
#include "../../include/filesystem/filesystem.h"
#include "../../ports/posix/fs_posix.h"
#include "../../include/reader.h"
#include "../../include/app.h"


static struct api_router api_router;
static struct api_route  routes[MAX_ROUTES];

static struct fs vfs_public;
static struct fs vfs_docs;

static struct static_router static_routers[2];
static struct redirect_registry redirects;
static struct redirect_rule redirect_rules[MAX_REDIRECTS];


static int read_all_from_stdin(uint8_t **buffer_out, size_t *buff_out_len) {
    const size_t chunk_size = 2048;
    uint8_t *buffer = NULL;
    size_t limit = chunk_size;
	size_t total_read = 0;
    uint8_t tmp[chunk_size];

    while(1){
        ssize_t curr_read = read_some(STDIN_FILENO, tmp, chunk_size);
        if(curr_read < 0){ 
			free(buffer); 
			return -1;
		}
        if(curr_read == 0) break;

		if ((size_t)curr_read > SIZE_MAX - total_read){
			free(buffer);
			return -1;
		}

        if (buffer == NULL) {
            buffer = calloc(1,limit);
            if (!buffer) return -1;
        }
        if(total_read + (size_t)curr_read > limit){
            while(limit < total_read + (size_t)curr_read){
                if(limit > SIZE_MAX / 2){
					free(buffer);
					return -1;
				}
				limit *= 2;
			}

            uint8_t *new_buffer = realloc(buffer, limit);
            if(!new_buffer){
				free(buffer);
				return -1;
			}
            buffer = new_buffer; 
        }
        memcpy(buffer + total_read, tmp, (size_t)curr_read);
        total_read += (size_t)curr_read;
    }
    *buffer_out = buffer; *buff_out_len = total_read;
    return 0;
}


static ssize_t write_all(int fd, const void *buffer, size_t len){
    size_t total_written = 0;
    while (total_written < len){
        ssize_t curr_written = write(fd, (const char*)buffer + total_written, len - total_written);
        if (curr_written < 0){
			if (errno == EINTR) continue; 
			return -1;
		}
        if (curr_written == 0) return -1;

		total_written += (size_t)curr_written;
    }
    return (ssize_t)total_written;
}


static int app_handler(const struct app_request *req, struct app_response *res) {
    struct redirect_result redirect_res = {0};
    int redirect_ret = redirect_lookup(&redirects, req->path, &redirect_res);
    if (redirect_ret == 0) return app_make_redirect(res, redirect_res.target, redirect_res.target_owned, redirect_res.type);
    if (redirect_ret < 0) return -1;

    int api_router_res = api_router_handle(&api_router, req, res);
    if (api_router_res == 0) return 0;
	if (api_router_res < 0) return -1;

    for (size_t i = 0; i < 2; i++){
    	int static_router_res = static_router_handle(&static_routers[i], req, res);
    	if (static_router_res == 0) return  0; 
		if (static_router_res < 0)  return -1;
    }

    static const char message[] = "Route not found\n";
    res->status = APP_NOT_FOUND;
    res->media_type = APP_MEDIA_TEXT;
    res->payload = message;
    res->payload_len = sizeof(message) - 1;
    res->payload_owned = false;
    res->redirect.enabled = false;
    return 0;
}


static void fuzz_one_request(const uint8_t *data, size_t size, void *ctx){
    if(!data || size == 0) return;
	int sv[2];
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) return;

	if(write_all(sv[1], data, size) < 0){
		close(sv[0]); 
		close(sv[1]); 
		return; 
	}

	shutdown(sv[1], SHUT_WR);
	http_handle_connection(sv[0], ctx);
	close(sv[0]);

	uint8_t response[600*1024];
	read(sv[1], response, sizeof(response));
	close(sv[1]);
	return;
}


int main(){
	const char public_root[] = "./public";
	const char docs_root[]   = "./docs";

	fs_init(&vfs_public, get_fs_ops(), public_root, sizeof(public_root)-1, NULL);
	fs_init(&vfs_docs,   get_fs_ops(), docs_root,   sizeof(docs_root)-1,   NULL);
	fs_ensure_dir(&vfs_public, "/", true);
	fs_ensure_dir(&vfs_docs,   "/", true);

	api_router_init(&api_router, "/api", routes, MAX_ROUTES);
	api_router_add(&api_router, APP_GET,  "/echo", handle_route_echo);
	api_router_add(&api_router, APP_POST, "/echo", handle_route_echo);

	static_router_init(&static_routers[0], "/public", &vfs_public, "index.html", 500*1024);
	static_router_init(&static_routers[1], "/docs",   &vfs_docs,   "index.html", 500*1024);

	redirect_registry_init(&redirects, redirect_rules, false, MAX_REDIRECTS, 0);
	redirect_add(&redirects, "/",             "/docs/",         EXACT, false, APP_REDIRECT_PERMANENT);
	redirect_add(&redirects, "/docs",         "/docs/",         EXACT, false, APP_REDIRECT_PERMANENT);
	redirect_add(&redirects, "/docs/doxygen", "/docs/doxygen/", EXACT, false, APP_REDIRECT_PERMANENT);
	redirect_add(&redirects, "/public",       "/public/",       EXACT, false, APP_REDIRECT_PERMANENT);

	struct app_adapter_ctx adapter_context = {0};
	adapter_context.app_handler = app_handler;

	struct http_core_ctx http_core_context = {
    	.adapter_handler = adapter_http_app,
    	.adapter_context = &adapter_context
	};

#ifdef __AFL_HAVE_MANUAL_CONTROL

    __AFL_INIT();
    __AFL_FUZZ_INIT();

    while(__AFL_LOOP(1000)){
		uint8_t *buffer = __AFL_FUZZ_TESTCASE_BUF;
		size_t len = (size_t)__AFL_FUZZ_TESTCASE_LEN;
		if(len > 0){
			fuzz_one_request(buffer, len, &http_core_context);
		}
	}
	return 0;

#else
    uint8_t *data = NULL;
    size_t size = 0;
    if (read_all_from_stdin(&data, &size) != 0) return 1;
    if (size > 0) fuzz_one_request(data, size, &http_core_context);
    free(data);
    return 0;
#endif
}
