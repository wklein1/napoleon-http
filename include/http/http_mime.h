#ifndef HTTP_MIME_H
#define HTTP_MIME_H

/**
 * @brief Struct containing a file extension and it's MIME type string.
 *
 * Represents a simple mapping of a file extension (e.g. "html", "json")
 * with the corresponding MIME type string as used in HTTP headers.
 */
struct mime_type {
    const char *ext; 	/**< File extension without leading dot (e.g. "html"). */
    const char *mime;	/**< MIME type string, including optional charset (e.g. "text/html; charset=UTF-8"). */
};

/**
 * @brief Look up the MIME type string for a given file extension.
 *
 * This function searches in the static MIME type mapping for the given
 * file extension and returns the corresponding MIME type string.
 *
 * @param extension
 *        File extension without leading dot (case-sensitive),
 *        e.g. `"html"`, `"css"`, `"png"`.  
 *
 * @return
 *        Pointer to a constant string containing the MIME type.
 *        Returns NULL If the extension is not found.
 *
 * @note
 *        The returned pointer refers to a static string and must
 *        not be freed or modified by the caller.
 */
const char* get_mime_type(const char* extension);

#endif /* HTTP_MIME_H */
