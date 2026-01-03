/*
 * Simple bcrypt wrapper for C++
 * Based on OpenBSD bcrypt implementation
 */

#ifndef _BCRYPT_H_
#define _BCRYPT_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate a bcrypt hash of a password
 * @param password Password to hash
 * @param cost Cost factor (4-31, recommended: 12)
 * @param hash Output buffer (must be at least 61 bytes)
 * @return 0 on success, -1 on error
 */
int bcrypt_hashpw(const char *password, int cost, char *hash);

/**
 * Verify a password against a bcrypt hash
 * @param password Password to verify
 * @param hash Hash to verify against
 * @return 0 if match, -1 if no match or error
 */
int bcrypt_checkpw(const char *password, const char *hash);

#ifdef __cplusplus
}
#endif

#endif /* _BCRYPT_H_ */
