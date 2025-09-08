#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/router/redirect_registry.h"

static ssize_t find_rule_index(const struct redirect_registry *registry, const char *from_path, const char *to_location, 
				 enum redirect_match_type match_type, bool append_tail, enum app_redirect_type redirect_type){

    for (size_t i = 0; i < registry->rule_count; i++) {
		const struct redirect_rule *rule = &registry->rules[i];
        if((strcmp(rule->from, from_path) == 0) &&
            (strcmp(rule->to, to_location) == 0) &&
			rule->match_type == match_type &&
            rule->append_tail == append_tail &&
			rule->redirect_type == redirect_type){
            	return (ssize_t)i;
        }
    }
    return -1;
}

static bool path_matches_segment_prefix(const char *path, size_t path_len, const char *prefix, size_t prefix_len){
    if(path_len < prefix_len) return false;
	if(strncmp(path, prefix, prefix_len) != 0) return false;
	if (path_len == prefix_len) return true;
	return path[prefix_len] == '/';
}
 
void redirect_registry_init(struct redirect_registry *registry, struct redirect_rule *rules, bool rules_owned,
							size_t capacity, size_t rule_count){
	registry->rules = rules;
	registry->rules_owned = rules_owned;
	registry->capacity = capacity;
	registry->rule_count = rule_count;
}

void redirect_registry_clear(struct redirect_registry *registry){
 	if(registry->rules && registry->rules_owned) free(registry->rules);
	registry->rules = NULL;
	registry->rules_owned = false;
	registry->capacity = 0;
	registry->rule_count = 0;
}

int redirect_add(struct redirect_registry *registry, const char *from_path, const char *to_location, 
				 enum redirect_match_type match_type, bool append_tail, enum app_redirect_type redirect_type){
	
	if(!registry || !from_path || !to_location) return -1;
	if(registry->rule_count >= registry->capacity) return -1;
	if(match_type == EXACT && append_tail) return -1;
	size_t from_len = strlen(from_path);
	if(from_len == 0) return -1;

	ssize_t rule_index = find_rule_index(registry, from_path, to_location, match_type, append_tail, redirect_type);
	if(rule_index!=-1) return -1;

	struct redirect_rule rule = {0};
	rule.from = from_path;
	rule.from_len = from_len;
	rule.to = to_location;
	rule.to_len = strlen(to_location);
	rule.match_type = match_type;
	rule.append_tail = append_tail;
	rule.redirect_type = redirect_type;

	registry->rules[registry->rule_count++] = rule;

	return 0;
}

int redirect_lookup(struct redirect_registry *registry, const char *path, struct redirect_result *result){
	if(!registry || !path || !result) return -1;
	
	size_t path_len = strlen(path);

	for(size_t i=0; i<registry->rule_count; i++){
		const struct redirect_rule *rule = &registry->rules[i];
		if(strcmp(path, rule->from)==0 && rule->match_type == EXACT){
			result->type = rule->redirect_type;
			result->target = rule->to;
			result->target_owned = false;
			return 0;
		}
	}

	ssize_t best_match_idx  = -1;
    size_t  best_match_len  =  0;
	int     best_match_prio =  0;

 	for (size_t i = 0; i < registry->rule_count; i++){
        const struct redirect_rule *rule = &registry->rules[i];
        bool matched = false;
		int prio;
        if (rule->match_type == PREFIX) {
            matched = (path_len >= rule->from_len && 
				strncmp(path, rule->from, rule->from_len) == 0);
			prio=1;
        } else if (rule->match_type == SEGMENT_PREFIX) {
            matched = path_matches_segment_prefix(path, path_len, rule->from, rule->from_len);
			prio=2;
        }else continue;
		
		if(!matched) continue;

        if ((rule->from_len > best_match_len) ||
			(rule->from_len == best_match_len && prio > best_match_prio) ||
			(rule->from_len == best_match_len && prio == best_match_prio && (ssize_t)i > best_match_idx))
		{
            best_match_idx = (ssize_t)i;
           	best_match_len = rule->from_len;
			best_match_prio = prio;
        }
    }

	if (best_match_idx < 0) return 1;

   const struct redirect_rule matched_rule = registry->rules[best_match_idx];
    if (!matched_rule.append_tail){
        result->target = matched_rule.to;
		result->target_owned = false;
        result->type = matched_rule.redirect_type;
        return 0;
    }

	const char *tail = path + matched_rule.from_len;
    const size_t tail_len = path_len - matched_rule.from_len;

	size_t target_len = matched_rule.to_len + tail_len+1;
	char *target_buffer = calloc(target_len, sizeof(char));
	if(!target_buffer) return -1;
    memcpy(target_buffer, matched_rule.to, matched_rule.to_len);
    memcpy(target_buffer + matched_rule.to_len, tail, tail_len);
    target_buffer[matched_rule.to_len + tail_len] = '\0';

	result->target = target_buffer;
	result->target_owned = true;
	result->type = matched_rule.redirect_type;

	return 0;
}
