#include "../../include/router/router_static.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>


/**
 * @brief Map a filename extension to an @ref app_media value.
 *
 * Expects @p ext to point to the leading dot of the extension
 * (e.g., ".html"). The comparison is **case-sensitive**.
 *
 * Recognized mappings:
 *  - ".html", ".htm" → APP_MEDIA_HTML
 *  - ".js"           → APP_MEDIA_JS
 *  - ".css"          → APP_MEDIA_CSS
 *  - ".txt"          → APP_MEDIA_TEXT
 *  - ".json"         → APP_MEDIA_JSON
 * Unknown or NULL → APP_MEDIA_BIN.
 *
 * @param ext Pointer to the extension substring (may be NULL).
 * @return Corresponding media type, or APP_MEDIA_BIN if unknown/NULL.
 */
static enum app_media media_from_ext(const char *ext) {
	
	if(!ext) return APP_MEDIA_BIN;
    if (!strcmp(ext, ".html") || !strcmp(ext, ".htm"))  	return APP_MEDIA_HTML;
    if (!strcmp(ext, ".js"))								return APP_MEDIA_JS;
    if (!strcmp(ext, ".css"))								return APP_MEDIA_CSS;
    if (!strcmp(ext, ".txt"))								return APP_MEDIA_TEXT;
    if (!strcmp(ext, ".json"))								return APP_MEDIA_JSON;

    return APP_MEDIA_BIN;
}


/**
 * @brief Find the extension in a path and return a pointer to the dot.
 *
 * Looks for the last '/' in @p path, then searches for the last '.'
 * after that position. If present, returns a pointer to the '.' inside
 * @p path; otherwise returns NULL.
 *
 * No allocation or copying is performed; the returned pointer remains
 * valid only as long as @p path is valid.
 *
 * @param path NUL-terminated path string (must not be NULL).
 * @return Pointer to the '.' starting the extension, or NULL if none.
 */
static const char* find_ext(const char *path) {
    const char *last_slash = strrchr(path, '/');
    const char *start = last_slash ? last_slash + 1 : path;
    const char *dot = strrchr(start, '.');
    return dot;
}


/**
 * @brief Build a docroot-relative path from a URL path and mount prefix.
 *
 * Behavior:
 *  - Skips a single '/' immediately after the prefix (if present).
 *  - Truncates at the first '?' or '#' (query/fragment start).
 *  - If the remaining part is empty or ends with '/', appends @p index_name.
 *  - Two-phase API:
 *      * If @p out_cap == 0: return the required length (excluding the NUL)
 *        and do not write to @p rel_path_out.
 *      * Otherwise: write the path into @p rel_path_out, NUL-terminate it,
 *        and return the length written (excluding the NUL).
 *
 * Notes:
 *  - The caller is responsible for ensuring that @p path actually matches
 *    the prefix and for any security checks beyond query/fragment stripping.
 *
 * @param path          Full URL path (NUL-terminated, must not be NULL).
 * @param path_len      Length of @p path in bytes (excluding the NUL).
 * @param prefix_len    Length of the prefix within @p path.
 * @param index_name    Default filename for directory requests (e.g., "index.html").
 * @param rel_path_out  Output buffer (may be NULL only when @p out_cap == 0).
 * @param out_cap       Capacity of @p rel_path_out in bytes, including space for the NUL.
 *
 * @return Required length (>= 0) on success; -1 on error
 *         (invalid args, insufficient capacity, or @p path_len <= @p prefix_len).
 */
static ssize_t build_rel_path(const char *path, size_t path_len, size_t prefix_len, 
						  const char *index_name, char *rel_path_out, size_t out_cap){

	if(!path || !index_name) return -1;
	if(path_len <= prefix_len) return -1;

	const char *p =  path + prefix_len;
	size_t remaining = path_len - prefix_len;

    if (remaining && *p == '/') { p++; remaining--; }

	size_t end = 0;

	while(end<remaining && p[end] != '#' && p[end] != '?') end++;

    size_t index_name_len = strlen(index_name);
	size_t rel_path_len = 0;

	if(end == 0 || p[end-1] == '/'){
		rel_path_len = end + index_name_len;
	}else{
		rel_path_len = end;
	}

	if(out_cap == 0) return rel_path_len;
	if(!rel_path_out) return -1;

	if(rel_path_len + 1 > out_cap) return -1;

	memcpy(rel_path_out, p, end);
	if (end == 0 || p[end - 1] == '/') {
        memcpy(rel_path_out + end, index_name, index_name_len);
    }
	rel_path_out[rel_path_len] = '\0';
	return rel_path_len;
}


void static_router_init(struct static_router *router, const char *prefix, struct fs *vfs, 
						const char *index_name, size_t max_bytes){
	if(!router || !vfs) return;
	router->prefix = prefix ? prefix : "/public";
	router->vfs = vfs;
	router->index_name = index_name ? index_name : "index.html";
	router->max_bytes = max_bytes;
}


int static_router_handle(struct static_router *router, const struct app_request *req,
                         struct app_response *out){
	
	size_t path_len = strlen(req->path);
	size_t prefix_len = strlen(router->prefix);
	if(path_len <= prefix_len) return 1; //TODO: redirect wenn path = prefix without trailing /
	if(prefix_len > 0){
        if (strncmp(req->path, router->prefix, prefix_len) != 0) {
            return 1;
        }

        char next_char = req->path[prefix_len];
        if (next_char != '\0' && next_char != '/') {
            return 1;
        }
    }

	if (req->method != APP_GET) {
        static const char mna_message[] = "Method not allowed\n";
        out->status        = APP_METHOD_NOT_ALLOWED;
        out->media_type    = APP_MEDIA_TEXT;
        out->payload       = mna_message;
        out->payload_len   = sizeof(mna_message) - 1;
        out->payload_owned = false;
        return 0;
    }

	ssize_t to_write = build_rel_path(req->path, path_len, prefix_len,
                                  router->index_name, NULL, 0);
    if(to_write < 0) return -1;


    char *rel_path = calloc((size_t)to_write + 1, sizeof(char));
    if(!rel_path) return -1;

    ssize_t wrote = build_rel_path(req->path, path_len, prefix_len,
                                   router->index_name, rel_path, (size_t)to_write + 1);

    if(wrote < 0 || wrote != to_write){ 
		free(rel_path); 
		return -1; 
	}

	struct fs_stat stat = {0};
    int stat_ret = fs_stat(router->vfs, rel_path, &stat);
    if (stat_ret != FS_OK || stat.node_type != FS_NODE_FILE) {
        static const char nf_message[] = "Not found\n";
        out->status        = APP_NOT_FOUND;
        out->media_type    = APP_MEDIA_TEXT;
        out->payload       = nf_message;
        out->payload_len   = sizeof(nf_message) - 1;
        out->payload_owned = false;
        free(rel_path);
        return 0;
    }

	if (stat.size > SIZE_MAX || (router->max_bytes && stat.size > router->max_bytes)) {
        static const char tl_message[] = "File too large\n";
        out->status        = APP_FORBIDDEN;
        out->media_type    = APP_MEDIA_TEXT;
        out->payload       = tl_message;
        out->payload_len   = sizeof(tl_message) - 1;
        out->payload_owned = false;
        free(rel_path);
        return 0;
    }

	
	struct fs_file *file = NULL;
	void *buffer = NULL;
	ssize_t total_read = 0;
	if(stat.size >0){
    	int open_ret = fs_open(router->vfs, rel_path, &file);
    	if(!file || open_ret != FS_OK){
			free(rel_path);
			return -1;
		}

		buffer = calloc(stat.size, 1);
		if(!buffer){
        	free(rel_path);
			fs_close(file);
        	return -1;
		}

		total_read = fs_read_all(file, buffer, stat.size);
		if(total_read < 0){
			free(buffer);
			free(rel_path);
			fs_close(file);
			return -1;
		}
		fs_close(file);
	}

	enum app_media media_type = media_from_ext(find_ext(rel_path));

    out->status        = APP_OK;
    out->media_type    = media_type;
    out->payload       = buffer;
    out->payload_len   = (size_t)total_read;
    out->payload_owned = buffer ? true : false;

	free(rel_path);
	return 0;
}
