#ifndef PUBLIC_COOKIES_H_
#define PUBLIC_COOKIES_H_

#include "buffers.h"
#include "err.h"



typedef struct __MCFW_COOKIE_SETTINGS__ {
    u64 max_age;
    bool http_only;
    bool same_site_strict;
    bool secure;
    DStatic path;
} CookieSettings;


#define COOKIE_TTL_SESSION 0          // Expires when browser closes
#define COOKIE_TTL_DAY     86400      // 24 hours
#define COOKIE_TTL_WEEK    604800     // 7 days
#define COOKIE_TTL_MONTH   2592000    // 30 days
#define COOKIE_TTL_YEAR    31536000   // 365 days

/**
 * Template for High-Security / Authorization Cookies
 * Use for: Session tokens, auth identifiers.
 * Protection: XSS-proof (HttpOnly), CSRF-proof (Strict), HTTPS-only.
 */
#define COOKIE_STRICT(_max_age) ((CookieSettings){    \
    .max_age = (_max_age),                            \
    .http_only = true,                                \
    .secure = true,                                   \
    .same_site_strict = true,                         \
    .path = DS_LIT("/")                               \
})

/**
 * Template for Non-Critical / Preference cookies.
 * Examples: UI theme (dark/light), locale/language, dismissed banners.
 * Protection: HTTPS-only, readable from JS (http_only = false), Lax navigation.
 */
#define COOKIE_LAX(_max_age) ((CookieSettings){         \
    .max_age = (_max_age),                              \
    .http_only = false,                                 \
    .secure = true,                                     \
    .same_site_strict = false,                          \
    .path = DS_LIT("/")                                 \
})


typedef struct __MCFW_HTTP_RESPONSE__ HttpResponse;

void cookie_set(HttpResponse* res, DStatic name, CookieSettings cs);

#endif // PUBLIC_COOKIES_H_