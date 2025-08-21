#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>
#include <libgen.h>
#include "../include/server.h"
#include "../include/app.h"


int parse_port(const char *input, uint16_t *output) {
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

    return server_start(&server_cfg, app_handle_client);
}
