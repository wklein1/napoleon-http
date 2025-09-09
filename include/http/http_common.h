#ifndef HTTP_COMMON_H
#define HTTP_COMMON_H

#include <stdbool.h>

/**
 * @struct http_header
 * @brief One header line (name: value).
 *
 * Ownership:
 * - If @ref name_owned is true, the framework/parser will free(@ref name).
 * - If @ref value_owned is true, the framework/parser will free(@ref value).
 * - If false, the pointers must refer to static/foreign storage and will not be freed.
 *
 * Note: 
 *   This struct is used by both request and response code paths.
 *   Strings are NUL-terminated. Ownership is indicated per field via *_owned flags.
 *   If a field’s *_owned flag is true, the respective pointer must be heap-allocated
 *   and will be freed by the corresponding clear() routine.
 */
struct http_header {
    const char *name;        /**< Header field name (e.g., "Content-Type"), null-terminated. */
    const char *value;       /**< Header field value (e.g., "text/plain"), null-terminated. */
    bool name_owned;   /**< true if @ref name is heap-owned and should be freed.  */
	bool value_owned;  /**< true if @ref value is heap-owned and should be freed. */
};


#endif /* HTTP_COMMON_H */
