#ifndef HTTP_CORE_H
#define HTTP_CORE_H

#include "../http/http_request.h"
#include "../http/http_response.h"


/**
 * @brief Core glue that drives request parsing, adapter dispatch, and response writeout.
 *
 * The core reads/parses an inbound message into an @ref http_request, invokes an
 * adapter callback to obtain an @ref http_response, serializes the response, and
 * then clears all temporary resources.
 *
 * The core itself does not know about the application layer. Instead, an
 * adapter bridges between HTTP requests/responses and the protocol-neutral
 * application contract.
 */


/**
 * @struct http_core_ctx
 * @brief Callback context for the core request handler.
 *
 * Contains the adapter callback that translates a parsed HTTP request into
 * an HTTP response, and an opaque adapter context pointer that is forwarded
 * unchanged to the adapter.
 *
 * @note Ownership/lifetime contract:
 *  - The core will clear the request with @ref http_request_clear() after the
 *    adapter returns.
 *  - The core will clear the response with @ref http_response_clear() after it
 *    has been serialized (or on error). Therefore any buffers the adapter stores
 *    in the response (e.g., `body`, `extra_headers`) must be heap-allocated and
 *    safe to free, or left as NULL. Do not point to stack storage or string
 *    literals unless your @ref http_response_clear implementation is aware not
 *    to free them.
 */
struct http_core_ctx {
   /**
     * Adapter callback that converts a parsed request into a response.
     *
     * @param http_req        Parsed request (read-only; valid only during the call).
     * @param http_res_out    Response to be filled by the adapter (start in zeroed state).
     * @param adapter_context Opaque pointer passed through from @ref http_core_ctx.
     *
     * @return
     *  - >= 0 on success (response in @p http_res_out will be serialized),
     *  - < 0 on technical failure (the core may emit a generic error response).
     */
    int (*adapter_handler)(const struct http_request *http_req, struct http_response *http_res_out, void *adapter_context );
    
	/** Opaque user data forwarded to @ref adapter_handler on each invocation. */
	void* adapter_context;
};


/**
 * @brief Handle one HTTP connection on a client socket.
 *
 * This function is called by the server accept loop for each connected
 * client. It:
 *   - parses the HTTP request from the socket,
 *   - invokes the adapter callback to obtain a response,
 *   - sends the response,
 *   - and performs cleanup.
 *
 * @param client_fd Connected client socket file descriptor.
 * @param context   Pointer to a @ref http_core_ctx provided by the caller.
 *
 * @return 0 on success, -1 on parse or send errors, or adapter return code.
 */
int http_handle_connection(int client_fd, void *context);

#endif  /* HTTP_CORE_H */
