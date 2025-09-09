#ifndef ROUTE_HANDLERS_H
#define ROUTE_HANDLERS_H

#include "../app.h"


/**
 * @brief Route handlers for the application router.
 *
 * Handlers receive an app_request and fill an app_response.
 * The adapter/core serializes the response and manages ownership as documented.
 */

/**
 * @brief Echo route.
 *
 * Behavior:
 * - If the request has a body (`body_len > 0`): the response echoes the body
 *   unchanged (zero-copy). `res->media_type` is taken from `req->content_type`.
 *   Ownership is **not** transferred (`payload_owned = false`).
 * - If the request has no body: the response is a small text line
 *   `"<METHOD> <path>"`. A heap buffer is allocated, media is
 *   `APP_MEDIA_TEXT`, and ownership **is** transferred (`payload_owned = true`).
 *
 * Methods:
 * - Supported: `GET`, `POST`.
 * - Unsupported methods are treated as a **client error**:
 *   the handler sets `res->status = APP_BAD_REQUEST` and returns 0 (handled).
 *
 * Status:
 * - On success paths the handler sets `res->status = APP_OK`.
 * - For client errors it sets `APP_BAD_REQUEST` and still returns 0.
 *
 * Ownership & lifetime:
 * - Zero-copy branch: `res->payload` points into `req->body` (non-owning). The
 *   caller must ensure the request buffer remains valid until the response is sent.
 * - Formatted branch: buffer is allocated (e.g., `calloc`) and `payload_owned`
 *   is set to true so the framework can `free()` it after sending.
 *
 * @param req  [in]  Normalized app request (must not be NULL).
 * @param res  [out] App response to populate (must not be NULL).
 *
 * @return
 *   - `0`  if the request was handled (including client-error responses like 400),
 *   - `-1` on **technical failure** (e.g., allocation or formatting error). In that
 *          case the framework may send a generic server error and ignore @p res.
 */
int handle_route_echo(const struct app_request *req, struct app_response *res);

#endif /* ROUTE_HANDLERS_H */
