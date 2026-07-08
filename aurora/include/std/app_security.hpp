#ifndef AURORA_APP_SECURITY_HPP
#define AURORA_APP_SECURITY_HPP

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Permission Enforcement (8) ── */
int  aurora_app_sec_init(void);
int  aurora_app_sec_declare_permission(const char* perm);
int  aurora_app_sec_check_permission(const char* perm);
int  aurora_app_sec_enforce_permissions(int enforce);
int  aurora_app_sec_verify_signature(const char* app_path);
int  aurora_app_sec_sign_app(const char* app_path, const char* key_path);
int  aurora_app_sec_sanitize_input(const char* input, char* out, int out_size);
int  aurora_app_sec_validate_path(const char* path);

/* ── Content Security Policy (4) ── */
int  aurora_app_sec_csp_set(const char* policy);
const char* aurora_app_sec_csp_get(void);
int  aurora_app_sec_csp_check_url(const char* url);
int  aurora_app_sec_csp_reset(void);

#ifdef __cplusplus
}
#endif

#endif
