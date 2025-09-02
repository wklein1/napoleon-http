#ifndef ROUTER_DYNAMIC_H
#define ROUTER_DYNAMIC_H

#include "../app.h"

/**
 * @brief Small dynamic router with caller-provided storage and fixed capacity.
 *
 * The router holds a mutable registry of routes in memory that the caller
 * provides (array + capacity). Routes can be added at runtime until capacity
 * is reached; matching is linear in registration order (first match wins).
 */

/**
 * @def MAX_ROUTES
 * @brief Convenience default for small setups (not enforced by the API).
 *
 * Pass the actual capacity to @ref api_router_init via @p routes_cap.
 */
#define MAX_ROUTES 5


/**
 * @typedef api_route_handler
 * @brief Handler invoked when a route matches.
 *
 * @param req  Read-only request (non-NULL).
 * @param out  Response to populate (non-NULL).
 * @return 0 on success; <0 on handler error.
 */
typedef int (*api_route_handler)(const struct app_request *req,
                                    struct app_response *out);


/**
 * @struct api_route
 * @brief Route descriptor: method + exact path + handler.
 *
 * @note The @c path pointer is not copied and must outlive the router.
 */
struct api_route {
    enum app_method method;
    const char *path;
    api_route_handler handler;
};


/**
 * @struct api_router
 * @brief Mutable registry backed by caller-provided storage (fixed capacity).
 *
 * @note Ownership/lifetime:
 *  - @c routes points to storage owned by the caller; capacity is @c max_routes.
 *  - @c prefix and each route @c path are not copied; they must remain valid.
 *  - Not thread-safe for concurrent registration/handling.
 */
struct api_router {
    const char       *prefix;        /**< Path prefix (defaults to "/api"). */
    struct api_route *routes;        /**< Internal route registry array */
    size_t            route_count;   /**< Number of active entries. */
    size_t            max_routes;    /**< Total capacity of @c routes. */
};


/**
 * @brief Initialize an API router with a pre-allocated route table.
 * @param router       Router instance to initialize.
 * @param prefix       Path prefix, only requests whose path 
 *  				   starts with this prefix are considered. 
 * @param route_table  Array of routes.
 * @param routes_cap   Number of @ref api_route entries available in @p route_table.
 */
void api_router_init(struct api_router *router, const char *prefix,
                     struct api_route *route_table, size_t routes_cap);


/**
 * @brief Register (append) a new route.
 *
 * Adds an entry to the registry if capacity allows. 
 * The first matching route in registration order wins.
 *
 * @param router  Router (non-NULL).
 * @param method  Method to match.
 * @param path    Exact path to match (non-NULL; not copied).
 * @param handler Handler to invoke (non-NULL).
 *
 * @return 0 on success; -1 if arguments are invalid or the table is full.
 */
int api_router_add(struct api_router *router, enum app_method method,
                   const char *path, api_route_handler handler);


/**
 * @brief Route a request to the first matching handler.
 *
 * If a non-empty @ref api_router::prefix is set and @p req->path does not start
 * with it, returns 1 (no match). Otherwise searches the registry linearly for a
 * route whose method and exact path match and invokes its handler.
 *
 * @param router Api Router.
 * @param req    Request to route.
 * @param out    Response to populate when a handler runs.
 *
 * @return 0 if a handler ran and returned success;
 *         1 if prefix mismatch;
 *        -1 on internal/router error or if the handler returned <0.
 */
int api_router_handle(struct api_router *router, const struct app_request *req,
                      struct app_response *out);


#endif /* ROUTER_DYNAMIC_H */
