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
};

struct redirect_rule {
    char *from;
    char *to;
    enum redirect_match_type match_type;
    enum app_redirect_type redirect_type;
};

struct redirect_registry {
    struct redirect_rule *rules;
    size_t rule_count;
    size_t capacity;
};


struct redirect_result {
    const char *target;
    enum app_redirect_type type;
    bool target_owned;
};


int  redirect_registry_init(struct redirect_registry *registry, size_t capacity);

void redirect_registry_clear();

int redirect_add(const char *from_path, const char *to_location, enum redirect_match_type match_type,
                 enum app_redirect_type type);

bool redirect_remove(const char *from_path, const char *to_location, enum redirect_match_type match_type);


bool redirect_lookup(const char *path, struct redirect_result *out);

#endif /* REDIRECT_REGISTRY_H */
