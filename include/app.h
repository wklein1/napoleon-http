#ifndef APP_H
#define APP_H

/**
 * @brief Business logic handler for client connections.
 *
 * This function processes a single client connection. It is intended
 * to be passed as a callback to the server, which provides the client
 * socket file descriptor.
 *
 * @param client_fd File descriptor of the accepted client socket.
 *
 * @return 0 on success, -1 on error.
 */
int app_handle_client(int client_fd);

#endif /* APP_H */
