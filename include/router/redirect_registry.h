#ifndef REDIRECT_REGISTRY_H
#define REDIRECT_REGISTRY_H

/**
 * @brief Small, transport-agnostic redirect registry: rule storage, lookup, and helpers.
 *
 * The registry holds a flat array of redirect rules and provides:
 *  - insertion with basic validation and duplicate prevention,
 *  - lookup by path with longest-prefix-wins semantics,
 *  - optional tail appending.
 */


#include <stddef.h>
#include <stdbool.h>
#include "../redirect/redirect_types.h"


/**
 * @enum redirect_match_type
 * @brief How an incoming path is matched against a rule's @ref redirect_rule::from.
 *
 * - @ref EXACT:          Path must match exactly.
 * - @ref PREFIX:         Path must begin with @c from (simple byte prefix).
 * - @ref SEGMENT_PREFIX: Path must begin with @c from and the next character is
 *                        either @c '\0' or @c '/' (segment boundary). This means
 *                        "/docs" matches "/docs", "/docs/", "/docs/index.html",
 *                        but not "/docsify".
 */
enum redirect_match_type {
    EXACT, 				/**< Exact path equality. */
    PREFIX,				/**< Simple byte prefix (no boundary check). */
	SEGMENT_PREFIX, 	/**< Prefix ending at a path segment boundary. */
};


/**
 * @struct redirect_rule
 * @brief A single redirect rule.
 *
 * Ownership & lifetime:
 *  - @ref from and @ref to point to externally owned, NUL-terminated strings
 *    (e.g., literals or configuration buffers). The registry does not free them.
 *  - @ref from_len and @ref to_len cache the byte lengths (excluding NUL) for faster lookup.
 *
 * Semantics:
 *  - If @ref append_tail is true and the rule matches via PREFIX/SEGMENT_PREFIX,
 *    the unmatched suffix of the request path ("tail") may be appended to @ref to
 *    by the lookup implementation. EXACT rules must not use @ref append_tail.
 *  - @ref redirect_type is an app-level semantic (temporary/permanent; with/without
 *    method preservation).
 */
struct redirect_rule {
    const char *from;
    const char *to;
	size_t from_len;
	size_t to_len;
    enum redirect_match_type match_type;
    enum app_redirect_type redirect_type;
	bool append_tail;
};


/**
 * @struct redirect_registry
 * @brief Flat container for redirect rules.
 *
 * Ownership:
 *  - If @ref rules_owned is true, @ref redirect_registry_clear will free the @ref rules array
 *    (but never the strings referenced by individual rules).
 *  - The rules themselves are stored by value in the array.
 */
struct redirect_registry {
    struct redirect_rule *rules; /**< Backing array for rules (may be static or heap). */
    bool   rules_owned;          /**< true → clear() frees @ref rules pointer. */
    size_t rule_count;           /**< Number of valid entries in @ref rules. */
    size_t capacity;             /**< Maximum number of rules the array can hold. */
};


/**
 * @struct redirect_result
 * @brief Result of a registry lookup.
 *
 * Ownership:
 *  - If @ref target_owned is true, the caller must free(@ref target).
 *    This is typically used when a rule was configured with @ref redirect_rule::append_tail
 *    and the implementation had to allocate a joined string.
 */
struct redirect_result {
    const char *target;               /**< Final redirect target to use. */
    enum app_redirect_type type;      /**< Redirect semantics to apply. */
    bool target_owned;                /**< true → caller must free(target). */
};


/**
 * @brief Initialize a redirect registry over a caller-provided rule array.
 *
 * Does not allocate memory; simply wires the registry to @p rules and sets
 * the initial counters. The array may be stack-, static-, or heap-allocated.
 *
 * @param registry    Registry to initialize (must not be NULL).
 * @param rules       Backing array for rules (may be NULL only if capacity==0).
 * @param rules_owned If true, @ref redirect_registry_clear will free @p rules.
 * @param capacity    Maximum number of rules the array can hold.
 * @param rule_count  Number of valid entries initially present (<= capacity).
 */
void redirect_registry_init(struct redirect_registry *registry, struct redirect_rule *rules, bool rules_owned,
							size_t capacity, size_t rule_count);


/**
 * @brief Reset a registry and free owned storage.
 *
 * Frees @ref redirect_registry::rules if @ref redirect_registry::rules_owned is true,
 * zeroes counters, and nulls pointers. Strings referenced by individual rules are
 * never freed by this function.
 *
 * @param registry Registry to clear (must not be NULL).
 */
void redirect_registry_clear(struct redirect_registry *registry);


/**
 * @brief Add a redirect rule to the registry.
 *
 * Validation rules:
 *  - @p registry, @p from_path and @p to_location must be non-NULL.
 *  - @p from_path must be non-empty.
 *  - Capacity must not be exceeded.
 *  - A rule is rejected if an *identical* rule already exists (same fields).
 *  - EXACT rules with @p append_tail == true are invalid and rejected.
 *
 * Lengths for @ref redirect_rule::from_len and @ref redirect_rule::to_len are
 * computed internally from the provided strings.
 *
 * @param registry     Target registry (must not be NULL).
 * @param from_path    Source path to match (absolute path).
 * @param to_location  Redirect target (absolute path or absolute URL).
 * @param match_type   Match strategy (EXACT/PREFIX/SEGMENT_PREFIX).
 * @param append_tail  true → append unmatched tail (PREFIX/SEGMENT_PREFIX only).
 * @param redirect_type Redirect semantics (temporary/permanent; preserve method, etc.).
 *
 * @return 0 on success; -1 on invalid arguments, duplicates, or capacity exceeded.
 */
int redirect_add(struct redirect_registry *registry, const char *from_path, const char *to_location, 
				 enum redirect_match_type match_type, bool append_tail, enum app_redirect_type redirect_type);


/**
 * @brief Find the best matching redirect for @p path.
 *
 * Matching semantics:
 *  1. If an EXACT rule matches, it is returned immediately.
 *  2. Otherwise, among PREFIX and SEGMENT_PREFIX rules:
 *     - Longest @ref redirect_rule::from_len wins ("longest prefix wins").
 *     - If lengths tie, SEGMENT_PREFIX outranks PREFIX.
 *     - If still tied, the later rule in the table wins (highest index).
 *
 * Result semantics:
 *  - On a match, @p result is filled with @ref target and @ref type.
 *    If the winning rule has @ref redirect_rule::append_tail == true,
 *    the implementation may allocate a joined string and set
 *    @ref redirect_result::target_owned = true; the caller must then free it.
 *
 * Return values:
 *  - 0  → match found and @p result filled.
 *  - 1  → no matching rule.
 *  - -1 → invalid input or allocation failure.
 *
 * @param registry Registry to query (must not be NULL).
 * @param path     Lookup path (NUL-terminated).
 * @param result   Output structure to fill on success (must not be NULL).
 *
 * @return 0 on match, 1 on no match, -1 on error.
 */
int redirect_lookup(struct redirect_registry *registry, const char *path, struct redirect_result *result);

#endif /* REDIRECT_REGISTRY_H */
