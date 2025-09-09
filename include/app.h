#ifndef APP_H
#define APP_H

#include <stddef.h>
#include <stdbool.h>
#include "./redirect/redirect_types.h"

/**
 * @brief Protocol-agnostic application.
 *
 * A transport/protocol adapter translates inbound messages into an
 * @ref app_request and calls @ref app_handle_client. The application fills an
 * @ref app_response which the adapter then serializes back to the client.
 *
 * @note All pointers are treated as read-only by the framework/adapter.
 *       The application must ensure their lifetime until the adapter has
 *       completed sending the response.
 */


/**
 * @def MAX_REDIRECTS
 * @brief Compile-time capacity for static redirect configuration.
 *
 * This value is used to size the application’s redirect rule table (e.g., a
 * fixed-size array) and is passed into the redirect registry to set its capacity.
 */
#define MAX_REDIRECTS 5 


/**
 * @brief Opaque virtual filesystem handle.
 *
 * Forward declaration; the full definition lives in @ref filesystem.h.
 * The application treats this as an immutable VFS environment used by
 * the static-file router. Ownership is NOT transferred: the pointer
 * passed to @ref app_init must remain valid for the duration of request
 * handling (typically until process shutdown).
 */
struct fs;


/**
 * @def MAX_STATIC_ROUTERS
 * @brief Maximum number of static mounts (routers) the app will register.
 */
#define MAX_STATIC_ROUTERS 8


/**
 * @struct app_mount
 * @brief Describes a static mount (URL prefix → VFS + directory defaults).
 *
 * Each mount instantiates one static-file router. Requests whose path begins
 * with @ref prefix are served from @ref vfs, using @ref index_name for
 * directory requests and honoring @ref max_bytes as a limit.
 */
struct app_mount {
    const char *prefix;      /**< URL prefix, e.g. "/docs" */
    struct fs  *vfs;         /**< Filesystem backing this mount; must outlive the app. */
	const char *index_name;  /**< Directory default, e.g. "index.html" (NULL → "index.html"). */
	size_t      max_bytes;   /**< Max file size to serve (bytes); 0 → no explicit limit. */
};

/**
 * @enum app_method
 * @brief Normalized operation verb.
 *
 * The names mirror common REST-style verbs but are not tied to any
 * particular protocol. The adapter is free to map transport-specific
 * methods to these values; unknown verbs should use @ref APP_OTHER.
 */
enum app_method{
    APP_GET,     /**< Read/retrieve. */
    APP_POST,    /**< Create/submit. */
    APP_PUT,     /**< Replace/update. */
    APP_DELETE,  /**< Remove. */
    APP_OTHER    /**< Any other/unknown verb. */
};


/**
 * @enum app_media
 * @brief Logical media classification for payload interpretation.
 *
 * The adapter may use this to choose representation details in the
 * serialized message (e.g., metadata or content-type) or to select
 * sensible defaults when none is provided.
 */
enum app_media{
    APP_MEDIA_NONE, /**< Unspecified/none. */
    APP_MEDIA_HTML, /**< Markup/HTML (UTF-8). */
	APP_MEDIA_JS,   /**< JavaScript source code. */
	APP_MEDIA_CSS,  /**< Cascading Style Sheets. */
    APP_MEDIA_TEXT, /**< Human-readable text (UTF-8). */
    APP_MEDIA_JSON, /**< Structured JSON (UTF-8). */
    APP_MEDIA_BIN   /**< Arbitrary binary data. */
};


/**
 * @enum app_status
 * @brief High-level outcome classification.
 *
 * The adapter decides how to express these outcomes in the transport
 * protocol (e.g., status codes or equivalents).
 */
enum app_status{
    APP_OK, 				/**< Successful result. */
    APP_CREATED,			/**< Resource created. */
    APP_NO_CONTENT, 		/**< Successful, no payload. */
    APP_BAD_REQUEST, 		/**< Client input invalid. */
    APP_FORBIDDEN,  		/**< Action not permitted. */
    APP_NOT_FOUND,			/**< Target not found. */
	APP_METHOD_NOT_ALLOWED, /**< Method is not allowed for the target resource. */
    APP_UNSUPPORTED, 		/**< Unsupported media/operation. */
    APP_ERROR				/**< Generic server/application error. */
};



/**
 * @struct app_redirect
 * @brief Optional redirect signaled by the application.
 *
 * If @ref enabled is true, the app indicates that the requested resource is located at
 * @ref location with semantics given by @ref type. Callers that support redirects should
 * emit a redirect instead of a regular payload; regular payload fields in
 * @ref app_response should be ignored in that case.
 *
 * Ownership:
 *  - If @ref location_owned is true, the caller must free(@ref location) after use.
 */
struct app_redirect {
    bool                    enabled;         /**< true → send a redirect instead of a payload. */
    const char             *location;        /**< Target location (absolute URL or absolute path). */
    bool                    location_owned;  /**< true if location must be freed */
    enum app_redirect_type  type;            /**< redirect semantics */
};


/**
 * @struct app_request
 * @brief Request forwarded to the application.
 * 
 * Populated by the adapter.
 */
struct app_request{
    enum app_method 	method;      	/**< App method classification */
    const char 			*path;          /**< Opaque resource identifier as provided by the adapter. */
    const void 			*payload;       /**< Request payload (read-only; may be NULL). */
    size_t 				payload_len;    /**< Payload length in bytes (may be 0). */
    enum app_media 		media_type;     /**< Media classification of @ref payload. */
    const char 			*accept;        /**< Optional client preference string (may be NULL). */
};


/**
 * @struct app_response
 * @brief Application response to be serialized by the adapter.
 *
 * Ownership & lifetime:
 * - If payload points to dynamically allocated memory and payload_owned == true,
 *   the adapter/framework will free it after sending.
 * - If payload points to static storage or memory owned elsewhere, set
 *   payload_owned == false (it will not be freed by the framework).
 * - For APP_NO_CONTENT, set payload_len to 0 and leave payload as NULL.
 */
struct app_response{
    enum app_status 	status; 		/**< Outcome status code. */
    enum app_media  	media_type;  	/**< Media classification of @ref payload. */
    const void 			*payload;    	/**< Response payload (read-only; may be NULL). */
    size_t 				payload_len;    /**< Payload length in bytes (0 if none). */
	bool 				payload_owned;  /**< true if framework should free(payload) after send. */
	struct app_redirect redirect;		/**< Optional redirect; takes precedence if enabled. */
};


/**
 * @brief Initialize application state (routers, handlers) from a mount array.
 *
 * Registers API routes and creates one static-file router per entry in @p mounts.
 * Intended to be called exactly once during process startup, before requests
 * are handled. The function is idempotent: subsequent successful calls return 0
 * and leave existing configuration intact.
 *
 * Requirements:
 *  - If @p mount_count > 0, @p mounts must be non-NULL.
 *  - Each mount must provide a non-NULL @ref app_mount::prefix and @ref app_mount::vfs.
 *  - @ref app_mount::index_name may be NULL to use "index.html".
 *  - @p mount_count must be ≤ @ref MAX_STATIC_ROUTERS.
 *
 * @param mounts       Array of static mount descriptors (may be NULL if count == 0).
 * @param mount_count  Number of entries in @p mounts.
 * @return 0 on success; -1 on invalid arguments or setup failure.
 */
int app_init(const struct app_mount *mounts, size_t mount_count);


/**
 * @brief Fill an @ref app_response to signal a redirect.
 *
 * Marks @ref app_response::redirect.enabled = true, stores @p location and @p type,
 * and clears regular payload fields. Callers that support redirects should emit one.
 *
 * Ownership:
 *  - If @p location_owned is true, the caller must free(@p location) after use.
 *
 * @param res             Response to fill (must not be NULL).
 * @param location        Target location for the redirect (must not be NULL).
 * @param location_owned  Whether the caller should free(@p location).
 * @param type            Redirect semantics (temporary/permanent, preserve method or not).
 *
 * @return 0 on success; -1 on invalid arguments.
 */
int app_make_redirect(struct app_response *res, const char *location, bool location_owned,
					  enum app_redirect_type type);


/**
 * @brief Handle a single normalized request and produce a response.
 *
 * On return:
 *  - If a redirect is requested (@ref app_response::redirect.enabled), callers
 *    should emit a redirect.
 *  - Otherwise, @ref status, @ref media_type, @ref payload and @ref payload_len
 *    describe the response body (if any).
 *
 * @param app_req      [in]  Request data (must not be NULL).
 * @param app_res_out  [out] Response to be filled by the application (must not be NULL).
 * @return 0 on success (response in @p app_res_out is valid);
 *         <0 on internal error (allocation/logic failure).
 */
int app_handle_client(const struct app_request *app_req, struct app_response *app_res_out);

#endif /* APP_H */
