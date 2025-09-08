#ifndef REDIRECT_TYPES_H
#define REDIRECT_TYPES_H

/**
 * @file redirect_types.h
 * @brief Transport-agnostic redirect semantics.
 *
 * These values express the application's intent for a redirection. A transport
 * layer (HTTP, CoAP, custom, etc.) is responsible for mapping them to concrete
 * protocol-level responses/codes and cache directives.
 *
 * Dimensions captured:
 *  - Permanence: temporary vs. permanent relocation.
 *  - Method preservation: whether the follow-up request must keep the original
 *    request method (and body, if any).
 *
 * If a transport has no notion of method preservation, treat the *_PRESERVE
 * variants as equivalent to their non-preserve counterparts.
 */

/**
 * @enum app_redirect_type
 * @brief Semantic redirect kinds independent of any specific protocol.
 */
enum app_redirect_type {

	/**
     * @brief Temporary relocation; do not permanently cache or update references.
     *
     * Method/body preservation is not required; a client or transport may
     * rewrite the request method according to its own rules.
     */
    APP_REDIRECT_TEMPORARY,

	/**
     * @brief Permanent relocation; clients may update references; caches may persist.
     *
     * Method/body preservation is not required; a client or transport may
     * rewrite the request method according to its own rules.
     */
    APP_REDIRECT_PERMANENT,

	/**
     * @brief Temporary relocation with method/body preservation.
     *
     * The follow-up request to the new location must keep the original
     * request method and (if applicable) its body. Clients should not
     * permanently update references.
     */
    APP_REDIRECT_TEMPORARY_PRESERVE,

    /**
     * @brief Permanent relocation with method/body preservation.
     *
     * The follow-up request must keep the original method and body. Clients
     * may update references and caches may persist the redirection.
     */
    APP_REDIRECT_PERMANENT_PRESERVE,
};

#endif /* REDIRECT_TYPES_H */
