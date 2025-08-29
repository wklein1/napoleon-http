#ifndef ADAPTER_HTTP_APP_H
#define ADAPTER_HTTP_APP_H
#include "../http/http_request.h"
#include "../http/http_response.h"
#include "../app.h"

/**
 * @file adapter_http_app.h
 * @brief Bridge between HTTP and the protocol-neutral app.
 *
 * This module is the ONLY place that includes both HTTP types and app types.
 * It converts:
 *   HTTP(method/path/headers/body) → app_request,
 *   calls the app handler,
 *   then app_response → HTTP response (status, Content-Type, body).
 */

struct app_adapter_ctx {
    int (*app_handler)(const struct app_request *app_req, struct app_response *app_res);   /**< Application handler function. */
};

/**
 * @brief HTTP→App adapter function.
 *
 * Builds an @ref app_request from @ref http_request, invokes the app handler,
 * then converts the @ref app_response to an @ref http_response. Ownership of any
 * heap-allocated payload is respected via @ref app_response::owns_payload.
 *
 * @param req_http  Parsed HTTP request (not owned by adapter).
 * @param out_http  Response to be sent by the core (filled here).
 * @param ud        Pointer to @ref app_adapter_ctx.
 * @return 0 on success, non-zero on application-level failure.
 */
int adapter_http_app(const struct http_request *req,
                     struct http_response *res_out,
                     void *adapter_context);

#endif /* ADAPTER_HTTP_APP_H */
