#include "../include/app.h"
#include "../include/http.h"
#include <string.h>

int app_handle_client(int client_fd){
    const char *body = "Hello World\n";
    return send_response(client_fd, 200, "text/plain; charset=UTF-8",
                         body, strlen(body));
}
