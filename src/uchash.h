/* UserChoice hashes: the integrity values Windows 10/11 stores next to a default-app choice.
 *
 * Portable C (no Windows headers) so tests/uchash_test.c can run it on any host.
 * Strings are UTF-16 code units; both functions lower-case ASCII themselves, so pass the
 * "canonical" string exactly as built from its parts. Output is the 12-character Base64
 * text Windows stores in the `Hash` value.
 *
 *   uc_hash_legacy  HKCU\...\FileExts\<ext>\UserChoice\Hash                (Windows 10, older 11)
 *   uc_hash_latest  HKCU\...\FileExts\<ext>\UserChoiceLatest\Hash          (Windows 11 24H2+)
 *
 * Both return 1 on success, 0 on failure (the output is then empty).
 */
#ifndef UCHASH_H
#define UCHASH_H
#include <stddef.h>
#include <stdint.h>

#define UC_HASH_CCH 16

/* Longest input uc_hash_latest accepts (a real canonical string is about 200 characters). */
#define UC_LATEST_MAX_CCH 330

int uc_hash_legacy(const uint16_t *s, size_t n, char out[UC_HASH_CCH]);
int uc_hash_latest(const uint16_t *s, size_t n, char out[UC_HASH_CCH]);

/* Known-answer test against a hash taken from a real Windows 11 machine. 1 = the
 * implementation (tables and mixing) is intact; the program refuses to write if not. */
int uc_hash_selftest(void);

#endif
