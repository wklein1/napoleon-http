#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "error.h"

int main(){
    struct sockaddr_in addr; 
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3001);


    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    EXIT_ON_ERROR( server_sock, "socket" );
    EXIT_ON_ERROR( inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr),"pton" );
    EXIT_ON_ERROR( bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)),"bind" );
    EXIT_ON_ERROR( listen(server_sock, SOMAXCONN),"listen" );

    printf("listening on port %d\n", 3001);

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
            const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "\r\n"
	    "Hello World\r\n" 
	    "\r\n";
            write(client_sock, response, strlen(response));
            LOG_ON_ERROR( close(client_sock), "close client_sock");
        }

    }
    

    LOG_ON_ERROR( close(server_sock), "close server_sock");
    return 0;
}
