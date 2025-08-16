#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../include/server.h"
#include "../include/error.h"

int server_start(const struct server_config *config, int (*client_handler)(int)){
    if (!config || !client_handler){
		fprintf(stderr, "server_start: bad arguments\n");
		return -1; 
	}
	
	u_int16_t port = config->port;
	const char* host = config->host;
	const int backlog = SOMAXCONN;
    
	struct sockaddr_in addr;
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);


    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    EXIT_ON_ERROR( server_sock, 0, "socket" );
    EXIT_ON_ERROR( inet_pton(AF_INET, host, &addr.sin_addr), 1,"pton" );
    EXIT_ON_ERROR( bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)), 0 ,"bind" );
    EXIT_ON_ERROR( listen(server_sock, backlog), 0, "listen" );

    printf("listening on port %d\n", port);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while(1){
        
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);

        if(client_sock >= 0){
            client_len = sizeof(client_addr);
            char client_ip_str[INET_ADDRSTRLEN];
            int client_port = ntohs(client_addr.sin_port);
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, sizeof(client_ip_str));
            printf("accepted connection from %s:%d \n",client_ip_str, client_port);
			
			char buffer[100] = {0};
            read(client_sock, buffer, sizeof(buffer));
            printf("%s\n", buffer);
			
			client_handler(client_sock);
            
			LOG_ON_ERROR( close(client_sock), 0, "close client_sock");
        }

    }
    
    LOG_ON_ERROR( close(server_sock), 0, "close server_sock");
    
}


void server_stop(void){

}
