#include "../../include/http/http_mime.h"
#include <string.h>

static const struct mime_type mime_map[] = {
    { "txt",  "text/plain; charset=UTF-8" },
    { "html", "text/html; charset=UTF-8" },
    { "json", "application/json; charset=UTF-8" },
	{ "css",  "text/css; charset=UTF-8" },
	{ "js",   "text/javascript; charset=UTF-8" },
};

const char* get_mime_type(const char* extension){
	if(!extension){
		return NULL;
	}
	for (size_t i=0; i < (sizeof(mime_map)/sizeof(struct mime_type)); i++){
		if(strcmp(extension, mime_map[i].ext) == 0) return mime_map[i].mime;
	}
	return NULL;
};
