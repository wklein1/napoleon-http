#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>
#include <libgen.h>
#include "../include/server.h"
#include "../include/app.h"
#include "../include/core/http_core.h"
#include "../include/adapters/adapter_http_app.h"
#include "../include/filesystem/filesystem.h"
#include "../ports/posix/fs_posix.h"


static int parse_port(const char *input, uint16_t *output) {
    if (!input) return -1;

    errno = 0;
    char *end;
    unsigned long port = strtoul(input, &end, 10);

    if (end == input
        || *end != '\0'
        || errno == ERANGE
        || port > 65535
	) return -1;

    *output = (uint16_t)port;

    return 0;
}


int main(int argc, char** argv){

	uint16_t port = 3001;
	char *prog = basename(argv[0]);
    char usage_str[128];
	snprintf(usage_str, sizeof usage_str, "Usage: %s <PORT>\n", prog);

	if(argc > 2){
		fputs(usage_str, stderr);
		exit(-1);
	}
	else if(argc == 2){
		if(parse_port(argv[1], &port)<0){
			fputs(usage_str, stderr);
	    	exit(1);
		}
	}

	struct server_config server_cfg = {
        .host = "127.0.0.1",
        .port = port,
        .backlog = 128
    };


	struct app_adapter_ctx adapter_context = { 
		.app_handler = app_handle_client
	};

	struct http_core_ctx http_core_context = {
		.adapter_handler = adapter_http_app,
		.adapter_context = &adapter_context
	};


	const char public_root[] = "./public";
	const char docs_root[]   = "./docs";

	struct fs vfs_public = {0};
	struct fs vfs_docs   = {0};

	fs_init(&vfs_public, get_fs_ops(), public_root, sizeof(public_root)-1, NULL);
	fs_init(&vfs_docs,   get_fs_ops(), docs_root,   sizeof(docs_root)-1,   NULL);

	int dir_ret = fs_ensure_dir(&vfs_public, "/", true);
	if (dir_ret != FS_OK){
    	fprintf(stderr, "Could not find or create root dir %s\n", public_root);
    	exit(1);
	}

	dir_ret = fs_ensure_dir(&vfs_docs, "/", true);
	if (dir_ret != FS_OK){
    	fprintf(stderr, "Could not find or create root dir %s\n", docs_root);
		exit(1);
	}	

	const struct app_mount mounts[] = {
    	{"/public", &vfs_public, "index.html", 500 * 1024},
    	{"/docs",   &vfs_docs,   "index.html", 500 * 1024},
	};
	if(app_init(mounts, 2) <0) exit(-1);

    return server_start(&server_cfg, http_handle_connection, &http_core_context);
}
