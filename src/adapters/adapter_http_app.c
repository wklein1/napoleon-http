#include <string.h>
#include <strings.h>
#include "../../include/adapters/adapter_http_app.h"
#include "../../include/http/http_request.h"
#include "../../include/http/http_response.h"

/**
 * @brief Map a method string to @ref app_method.
 *
 * Performs a case-sensitive match on common verbs ("GET", "POST", "PUT", "DELETE").
 * Unknown or NULL inputs map to @ref APP_OTHER.
 *
 * @param method NUL-terminated method string (may be NULL).
 * @return Corresponding @ref app_method value, or @ref APP_OTHER for unknown.
 */
static enum app_method map_method(const char *method){
    if (!method) return APP_OTHER;
    if (!strcmp(method, "GET"))    return APP_GET;
    if (!strcmp(method, "POST"))   return APP_POST;
    if (!strcmp(method, "PUT"))    return APP_PUT;
    if (!strcmp(method, "DELETE")) return APP_DELETE;
    return APP_OTHER;
}


/**
 * @brief Map a Content-Type string to @ref app_media.
 *
 * Case-insensitive prefix checks are used and parameters are ignored, e.g.
 * "application/json; charset=UTF-8" → @ref APP_MEDIA_JSON.
 * Recognized prefixes:
 *  - "application/json"
 *  - "text/plain"
 *  - "text/html"
 *  - "application/octet-stream"
 *
 * @param content_type NUL-terminated Content-Type header value (may be NULL).
 * @return Matching @ref app_media value, or @ref APP_MEDIA_NONE if unknown/NULL.
 */
static enum app_media media_from_content_type(const char *content_type){
    if (!content_type) return APP_MEDIA_NONE;
    if (!strncasecmp(content_type, "application/json", 16))				return APP_MEDIA_JSON;
    if (!strncasecmp(content_type, "text/plain", 10))					return APP_MEDIA_TEXT;
    if (!strncasecmp(content_type, "text/html", 9))						return APP_MEDIA_HTML;
    if (!strncasecmp(content_type, "application/octet-stream", 24)) 	return APP_MEDIA_BIN;
    return APP_MEDIA_NONE;
}


/**
 * @brief Map an @ref app_media to a canonical HTTP Content-Type string.
 *
 * Returns a constant Content-Type string for common media.
 * Text types include a UTF-8 charset.
 * For @ref APP_MEDIA_NONE or unknown values, returns NULL so the caller can
 * apply a default.
 *
 * @param media Media classification.
 * @return Constant MIME type string (static storage) or NULL if none.
 */
static const char* media_to_http_content_type(enum app_media media){
    switch (media) {
        case APP_MEDIA_HTML: return "text/html; charset=UTF-8";
        case APP_MEDIA_JS:   return "text/javascript";
        case APP_MEDIA_CSS:  return "text/css";
        case APP_MEDIA_JSON: return "application/json; charset=UTF-8";
        case APP_MEDIA_TEXT: return "text/plain; charset=UTF-8";
        case APP_MEDIA_BIN:  return "application/octet-stream";
        default: return NULL;
    }
}


/**
 * @brief Map an @ref app_status to an HTTP status code.
 *
 * Provides a straightforward mapping of high-level outcomes to numeric codes.
 * Unknown values fall back to 500.
 *
 * @param status Application outcome classification.
 * @return HTTP status code (e.g., 200, 404, 500).
 */
static int app_status_to_http_status(enum app_status status){
    switch (status) {
        case APP_OK:		  			return 200;
        case APP_CREATED:	  			return 201;
        case APP_NO_CONTENT:  			return 204;
        case APP_BAD_REQUEST: 			return 400;
        case APP_FORBIDDEN:				return 403;
        case APP_NOT_FOUND:				return 404;
        case APP_METHOD_NOT_ALLOWED:	return 405;
        case APP_UNSUPPORTED: 			return 415;
        case APP_ERROR:		  			return 500;
        default:			  			return 500;
    }
}


int adapter_http_app(const struct http_request *http_req, struct http_response *http_res_out, 
					 void *adapter_context){

    const struct app_request app_req = {
        .method		  = map_method(http_req->method),
        .path		  = http_req->path,
        .payload	  = http_req->body,
        .payload_len  = http_req->content_length,
        .media_type   = media_from_content_type(http_request_get_header_value(http_req, "Content-Type")),
        .accept		  = http_request_get_header_value(http_req, "Accept")
    };

    struct app_response app_res = {0};
    int app_ret = ((struct app_adapter_ctx*)adapter_context)->app_handler(&app_req, &app_res);

	http_res_out->status	   = app_status_to_http_status(app_res.status);
    http_res_out->content_type = media_to_http_content_type(app_res.media_type);
    http_res_out->body = app_res.payload;
    http_res_out->content_length  = app_res.payload_len;
	http_res_out->body_owned = app_res.payload_owned;

    return app_ret;
}
