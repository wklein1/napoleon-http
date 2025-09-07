#ifndef ROUTER_STATIC_H
#define ROUTER_STATIC_H

#include <stddef.h>
#include "../../include/app.h"
#include "../filesystem/filesystem.h"


/**
 * @brief Simple static-file router using the filesystem abstraction.
 *
 * Matches requests by URL prefix and serves files from a given filesystem root.
 * Only GET is handled. Results are returned as app_response with payload
 * allocated on the heap (payload_owned = true).
 */


/**
 * @struct static_router
 * @brief Configuration and state for the static-file router.
 */
struct static_router {
  const char *prefix;		/**< Path prefix (defaults to "/public"). */
  struct fs *vfs;			/**< Filesystem abstraction (already initialized) */
  const char *index_name;	/**< Default file for directories (defaults to "index.html") */
  size_t max_bytes;			/**< max file size to read into memory (0 = no limit) */
};


/**
 * @brief Initialize a static router.
 *
 * @param router		Router instance to fill (non-NULL).
 * @param prefix		URL prefix to match (may be NULL -> defaults to "/public").
 * @param vfs			Filesystem abstraction (non-NULL, already initialized).
 * @param index_name	Directory default (may be NULL -> defaults to "index.html").
 * @param max_bytes		Maximum allowed file size (0 means "no explicit limit").
 */
void static_router_init(struct static_router *router, const char *prefix, struct fs *vfs, 
						const char *index_name, size_t max_bytes);


/**
 * @brief Try to serve a request from the filesystem.
 *
 * Behavior:
 *  - If method is not GET, writes app 405 response, returns 0 (handled).
 *  - If path does not start with router's prefix, returns 1 (not handled).
 *  - If a matching file is found and within size limit, fills @pbeginnin res and returns 0
 *  - If no matching file is found, writes app 404 response, return 0 (handled).
 *  - On internal error (I/O, allocation, etc.) returns -1.
 *
 * @param router	Router (non-NULL).
 * @param req		App request (non-NULL).
 * @param res		App response to populate on success (non-NULL).
 *
 * @return 0 handled; 1 not handled; -1 internal error.
 */
int static_router_handle(struct static_router *router, const struct app_request *req,
                         struct app_response *res);

#endif /* ROUTER_STATIC_H */
