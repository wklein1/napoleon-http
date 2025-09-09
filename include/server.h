#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

/**
 * @brief Server configuration structure.
 *
 * Holds the basic configuration parameters required to start the server.
 */
struct server_config {
    const char *host;   /**< IP address or hostname */
    uint16_t    port;   /**< Port number to listen on */
    int         backlog;/**< Maximum number of queued connections */
};

/**
 * @brief Start the server accept loop.
 *
 * Opens a listening socket on the configured host and port. For each
 * accepted connection, the provided handler function is called with
 * the client-socket file descriptor.
 *
 * @param config     	 Pointer to server configuration structure (host, port, backlog).
 * @param client_handler Callback function for each accepted client, must not be NULL.
 * @param context		 Opaque pointer forwarded to @p client_handler on each call (may be NULL).
 *
 * @return 0 on success, -1 on error.
 *
 * @note The function typically blocks until @ref server_stop() is called or an error occurs.
 */
int server_start(const struct server_config *config, int (*client_handler)(int client_fd, void *context), void *context);

/**
 * @brief Stop the server.
 *
 * Exits the accept loop. Any open sockets are closed.
 */
void server_stop(void);

#endif /* SERVER_H */
