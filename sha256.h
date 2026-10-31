#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>

/* Writes a NUL-terminated lower-case SHA-256 hex digest (65 bytes). */
void password_digest(const char *password, const char *salt, char out[65]);

#endif
