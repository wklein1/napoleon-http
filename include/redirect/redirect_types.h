#ifndef REDIRECT_TYPES_H
#define REDIRECT_TYPES_H

/**
 * @brief Semantic redirect kind; the HTTP adapter maps this to a status code.
 *
 * Typical mapping:
 *  - APP_REDIRECT_PERMANENT          → 301
 *  - APP_REDIRECT_TEMPORARY          → 302
 *  - APP_REDIRECT_TEMPORARY_PRESERVE → 307 (preserve method)
 *  - APP_REDIRECT_PERMANENT_PRESERVE → 308 (preserve method)
 */
enum app_redirect_type {
    APP_REDIRECT_TEMPORARY,
    APP_REDIRECT_PERMANENT,
    APP_REDIRECT_TEMPORARY_PRESERVE,
    APP_REDIRECT_PERMANENT_PRESERVE,
};

#endif /* REDIRECT_TYPES_H */
