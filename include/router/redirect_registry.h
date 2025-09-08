#ifndef REDIRECT_REGISTRY_H
#define REDIRECT_REGISTRY_H

#include <stddef.h>
#include <stdbool.h>

enum app_redirect_type {
    APP_REDIRECT_TEMPORARY,
    APP_REDIRECT_PERMANENT,
    APP_REDIRECT_TEMPORARY_PRESERVE,
    APP_REDIRECT_PERMANENT_PRESERVE,
};

enum redirect_match_type {
    EXACT,
    PREFIX,
	SEGMENT_PREFIX,
};

struct redirect_rule {
    const char *from;
    const char *to;
	size_t from_len;
	size_t to_len;
    enum redirect_match_type match_type;
    enum app_redirect_type redirect_type;
	bool append_tail;
};

struct redirect_registry {
    struct redirect_rule *rules;
	bool rules_owned;
    size_t rule_count;
    size_t capacity;
};


struct redirect_result {
    const char *target;
    enum app_redirect_type type;
    bool target_owned;
};


void redirect_registry_init(struct redirect_registry *registry, struct redirect_rule *rules, bool rules_owned,
							size_t capacity, size_t rule_count);

void redirect_registry_clear(struct redirect_registry *registry);

int redirect_add(struct redirect_registry *registry, const char *from_path, const char *to_location, 
				 enum redirect_match_type match_type, bool append_tail, enum app_redirect_type redirect_type);

int redirect_lookup(struct redirect_registry *registry, const char *path, struct redirect_result *result);

#endif /* REDIRECT_REGISTRY_H */
