/*
 * UserChoice hashes (see uchash.h).
 *
 * Both hashes share one back end: MD5 of a 16-bit word stream, then two mixing passes
 * (A and B) over the same stream, XORed together and Base64-encoded.
 *
 *   legacy : the stream is the UTF-16 string plus its terminating NUL.
 *   latest : the stream is the output of a table-driven token encoder (a small
 *            LZ-style compressor that Windows runs over the string first).
 *
 * The encoder, tables and seeds are a C port of cssxn/UserChoiceLatestHash (MIT, see
 * LICENSE-THIRD-PARTY.txt). The legacy hash follows the original PS-SFTA algorithm.
 * tools/ref_hash.py is an independent Python transcription that tests/ checks this against.
 */
#include "uchash.h"
#include <stdlib.h>
#include <string.h>

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

#include "uchash_tables.inc"

#define SENTINEL   0xFBFFFFFFFFFFFFFFULL
#define TOK_A      0xFE00000000000000ULL
#define TOK_B      0xFE00800000000000ULL
#define TOK_C      0xFF00000000000000ULL
#define TOK_D      0xFE007DFFFFFFFFFFULL
#define TOK_E      0xFE00801F7FFFFFFFULL
#define SEEN_MOD   0x2803u
#define SEEN_WORDS 161

static const U32 SEED_A[4] = { 0x69FB0000u, 0x79F8A395u, 0x3CE8EC25u, 0x10FA9605u };
static const U32 SEED_B[4] = { 0x12CEB96Du, 0x2B890000u, 0x5B9F0000u, 0x2C7C6901u };

/* ------------------------------------------------------------------ MD5 */
static const uint32_t MD5_K[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
    0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
    0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
    0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};
static const U8 MD5_S[16] = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };

static U32 rol(U32 x, unsigned c) { return (x << c) | (x >> (32 - c)); }

static void md5_block(U32 st[4], const U8 *p)
{
    U32 m[16], a = st[0], b = st[1], c = st[2], d = st[3];
    for (int i = 0; i < 16; i++)
        m[i] = (U32)p[4 * i] | ((U32)p[4 * i + 1] << 8) | ((U32)p[4 * i + 2] << 16) | ((U32)p[4 * i + 3] << 24);
    for (int i = 0; i < 64; i++) {
        U32 f; int g;
        if (i < 16)      { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d;          g = (3 * i + 5) & 15; }
        else             { f = c ^ (b | ~d);       g = (7 * i) & 15; }
        U32 t = d; d = c; c = b;
        b = b + rol(a + f + MD5_K[i] + m[g], MD5_S[(i >> 4) * 4 + (i & 3)]);
        a = t;
    }
    st[0] += a; st[1] += b; st[2] += c; st[3] += d;
}

/* Digest of `n` bytes as four little-endian words (the way the mixers consume it). */
static int md5_words(const U8 *data, size_t n, U32 out[4])
{
    U32 st[4] = { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u };
    size_t i = 0;
    for (; i + 64 <= n; i += 64) md5_block(st, data + i);
    U8 tail[128]; size_t rem = n - i;
    memset(tail, 0, sizeof tail);
    memcpy(tail, data + i, rem);
    tail[rem] = 0x80;
    size_t tl = rem + 1 + 8 <= 64 ? 64 : 128;
    U64 bits = (U64)n * 8;
    for (int k = 0; k < 8; k++) tail[tl - 8 + k] = (U8)(bits >> (8 * k));
    md5_block(st, tail);
    if (tl == 128) md5_block(st, tail + 64);
    memcpy(out, st, sizeof st);
    return 1;
}

/* ---------------------------------------------------------------- tables */
static struct {
    U8 *cls, *region, *fallback, *bitw, *srw, *lrw;
    size_t ncls, nregion, nfallback, nbitw, nsrw, nlrw;
    int ready;
    int fail;                 /* set on any out-of-range table access or allocation failure */
} G;

static int hexv(char c)
{
    return c <= '9' ? c - '0' : (c | 32) - 'a' + 10;
}

static U8 *decode(const char *h, size_t *n)
{
    size_t len = strlen(h) / 2;
    U8 *b = (U8 *)malloc(len ? len : 1);
    if (!b) return NULL;
    for (size_t i = 0; i < len; i++) b[i] = (U8)((hexv(h[2 * i]) << 4) | hexv(h[2 * i + 1]));
    *n = len;
    return b;
}

static int tables_init(void)
{
    if (G.ready) return 1;
    G.region   = decode(kTokenRegionHex,            &G.nregion);
    G.lrw      = decode(kLongRewindTableHex,        &G.nlrw);
    G.fallback = decode(kFallbackTokenCodeTableHex, &G.nfallback);
    G.cls      = decode(kByteClassTableHex,         &G.ncls);
    G.bitw     = decode(kTokenBitWidthTableHex,     &G.nbitw);
    G.srw      = decode(kShortRewindTableHex,       &G.nsrw);
    G.ready = G.region && G.lrw && G.fallback && G.cls && G.bitw && G.srw;
    return G.ready;
}

#define TBL(name, arr, cnt) \
    static U32 name(size_t i) { if (i >= G.cnt) { G.fail = 1; return 0; } return G.arr[i]; }
TBL(t_region, region,   nregion)
TBL(t_cls,    cls,      ncls)
TBL(t_fb,     fallback, nfallback)
TBL(t_bw,     bitw,     nbitw)
TBL(t_srw,    srw,      nsrw)
TBL(t_lrw,    lrw,      nlrw)

static U32 class_of(U32 ch)
{
    U32 hi = ch >> 8;
    return hi ? ((t_cls(hi) + 8u) & 0xFFu) : t_cls(ch & 0xFFu);
}

/* ------------------------------------------------------- token encoder */
typedef struct {
    U64 *e; size_t n, cap;
    U64 seen[SEEN_WORDS];
    U64 last;
    U32 repeat, total;
} Enc;

static int enc_init(Enc *s)
{
    memset(s, 0, sizeof *s);
    s->cap = 4096;
    s->e = (U64 *)calloc(s->cap, sizeof(U64));
    if (!s->e) return 0;
    s->n = 1;                           /* slot 0 is the header, filled in at the end */
    s->last = SENTINEL;
    return 1;
}

static void mark_seen(Enc *s, U64 v)  { U32 i = (U32)(v % SEEN_MOD); s->seen[i >> 6] |= 1ULL << (i & 63); }
static int  test_seen(const Enc *s, U64 v) { U32 i = (U32)(v % SEEN_MOD); return (s->seen[i >> 6] >> (i & 63)) & 1; }

static void push(Enc *s, U64 v)
{
    if (s->n >= s->cap) {
        U64 *p = (U64 *)realloc(s->e, (s->cap + 1024) * sizeof(U64));
        if (!p) { G.fail = 1; return; }
        memset(p + s->cap, 0, 1024 * sizeof(U64));
        s->e = p; s->cap += 1024;
    }
    s->e[s->n++] = v;
}

static void put(Enc *s, U64 v) { mark_seen(s, v); push(s, v); }

static int find_prev(const Enc *s, U64 v, U64 *dist)
{
    if (!test_seen(s, v) || s->n == 0) return 0;
    size_t start = s->n > 0x400 ? s->n - 0x400 : 0;
    for (size_t i = s->n - 1;; i--) {
        if (s->e[i] == v) { *dist = s->n - i; return 1; }
        if (i == start) break;
    }
    return 0;
}

/* One token starting at `cur`; `len` is how many characters it covers. */
static int token_second(const U16 *in, long cur, long end, U64 *tok, U32 *kind, U32 *len)
{
    if (cur >= end) return 0;
    long remain = end - cur;
    U32 code = t_region(class_of(in[cur]) + 256u);
    U32 tl = t_region(code);
    if ((long)tl > remain) { code = t_fb((size_t)remain); tl = t_region(code); }
    size_t base = 168u * code;
    for (;;) {
        int retry = 0;
        if (tl != 0) {
            U32 pos = 0;
            for (long probe = cur; probe < end;) {
                if ((U64)in[probe] >= (1ULL << t_bw(base + pos))) {
                    code++; base = 168u * code; tl = t_region(code);
                    if ((long)tl <= remain) retry = 1;
                    break;
                }
                pos++; probe++;
                if (pos >= tl) break;
                if (G.fail) return 0;
            }
        }
        if (G.fail) return 0;
        if (!retry) break;
    }
    U64 t = (U64)(code & 0xFF) << 56;
    int shift = 56;
    for (U32 i = 0; i < (tl & 0xFF); i++) {
        if (cur + (long)i >= end) break;
        shift -= (int)t_bw(base + i);
        if (shift < 0) { G.fail = 1; return 0; }
        t |= (U64)in[cur + i] << shift;
    }
    *tok = t; *kind = code & 0xFF; *len = tl & 0xFF;
    return !G.fail;
}

/* Records one token, collapsing runs and back-references. Returns the (possibly rewound)
 * cursor. */
static long emit(Enc *s, long cur, U64 tok, U32 kind)
{
    U64 d;
    if (tok == s->last && s->repeat < 0xFF) { s->repeat++; return cur; }

    if (s->repeat != 0 && find_prev(s, s->last, &d)) {
        U32 rm1 = s->repeat - 1;
        U64 entry;
        if (d >= 0x800) {
            put(s, (tok >> 17) | ((U64)rm1 << 48) | TOK_A);
            entry = s->last;
        } else {
            entry = (tok >> 27) | ((U64)((((U32)(U16)d - 1u) & 0x3FFu) | (rm1 << 11)) << 37) | TOK_B;
        }
        put(s, entry);
        s->last = SENTINEL;
        s->repeat = 0;
        return cur - (long)(d >= 0x800 ? t_lrw(kind) : t_srw(kind));
    }
    put(s, tok);
    s->last = tok;
    return cur;
}

static void flush(Enc *s)
{
    U64 d, entry;
    if (s->repeat == 0) return;
    if (s->last == SENTINEL) {
        entry = ((U64)s->repeat << 48) | TOK_D;
    } else if (find_prev(s, s->last, &d) && d != 0) {
        entry = ((U64)((((U32)(U16)d - 1u) & 0x3FFu) | ((s->repeat - 1u) << 11)) << 37) | TOK_E;
    } else {
        put(s, ((U64)s->repeat << 48) | TOK_D);
        entry = s->last;
    }
    put(s, entry);
    s->repeat = 0;
    s->last = SENTINEL;
}

/* Tokenise `in` into the packed 16-bit stream the hash is computed over. Caller frees.
 *
 * For very long inputs Windows runs an extra leading pass. It only engages from 336
 * characters and a canonical string is about 200, so it is not implemented: longer inputs
 * are refused instead of being hashed wrongly. */
static U16 *encode(const U16 *in, size_t n, size_t *npacked)
{
    if (n > UC_LATEST_MAX_CCH) return NULL;
    Enc s;
    if (!enc_init(&s)) return NULL;
    long end = (long)n, cur = 0;
    for (long steps = 0; cur < end && !G.fail; steps++) {
        U64 tok; U32 kind, tl;
        if (steps > 200000 || !token_second(in, cur, end, &tok, &kind, &tl)) { G.fail = 1; break; }
        s.total += tl;
        cur += (long)tl;
        cur = emit(&s, cur, tok, kind);
        if (cur < 0) G.fail = 1;
    }
    if (G.fail) { free(s.e); return NULL; }
    flush(&s);
    s.e[0] = (U64)s.total | ((U64)s.n << 32);

    U16 *out = (U16 *)malloc((s.n * 4 + 1) * sizeof(U16));
    if (!out) { free(s.e); return NULL; }
    size_t k = 0;
    for (size_t i = 0; i < s.n; i++)
        for (int sh = 0; sh < 64; sh += 16) {
            U16 part = (U16)(s.e[i] >> sh);
            if (part) out[k++] = part;
        }
    out[k++] = 0;
    free(s.e);
    *npacked = k;
    return out;
}

/* ---------------------------------------------------------------- mixing */
static U32 lm(U32 a, U32 b) { return (U32)((U64)a * (U64)b); }

static int mix_a(const U32 *w, int count, const U32 md5[4], const U32 sd[4], U32 out[2])
{
    if (count & 1) return 0;
    U32 md0 = md5[0] | 1u, md1 = (md5[1] | 1u) + 0x13DB0000u;
    U32 sum = 0, acc = 0, mul = md0 + sd[0];
    for (int i = 0, loops = ((count - 2) >> 1) + 1; i < loops; i++) {
        acc += w[2 * i];
        U32 t  = lm(acc, mul) - lm(acc >> 16, sd[3]);
        U32 s  = lm(t, sd[1]) + lm(t >> 16, 0x689B6B9Fu);
        U32 p1 = lm(s, 0xEA970001u) - lm(s >> 16, 0x3C101569u);
        U32 wv = w[2 * i + 1] + p1;
        U32 t2 = lm(wv, md1) - lm(wv >> 16, sd[2]);
        U32 tp = lm(t2, 0x59C3AF2Du) - lm(t2 >> 16, 0x2232E0F1u);
        acc = lm(tp >> 16, 0x35BD1EC9u) + lm(tp, 0x1EC90001u);
        sum += p1 + acc;
    }
    out[0] = acc; out[1] = sum;
    return 1;
}

static int mix_b(const U32 *w, int count, const U32 md5[4], const U32 sd[4], U32 out[2])
{
    if (count & 1) return 0;
    U32 md0 = md5[0] | 1u, md1 = md5[1] | 1u;
    U32 sum = 0, acc = 0;
    for (int i = 0, loops = ((count - 2) >> 1) + 1; i < loops; i++) {
        U32 x = w[2 * i] + acc;
        U32 t = lm(x, md0);
        U32 a = lm(t, 0xB1110000u) - lm(t >> 16, 0x30674EEFu);
        U32 b = lm(a, sd[2]);
        U32 c = b - lm(a >> 16, 0x78F7A461u);
        c = lm(c >> 16, sd[0]) - lm(a >> 16, 0x164D0000u) - lm(b, 0x46930000u);
        U32 d = lm(c >> 16, 0x257E1D83u) + lm(c, 0x1D830000u);
        U32 e = w[2 * i + 1] + d;
        U32 f = lm(e, md1);
        U32 g = lm(f, 0x16F50000u) - lm(f >> 16, 0x5D8BE90Bu);
        U32 h = lm(g >> 16, sd[3]);
        U32 ii = lm(g, 0x69010000u) + h;
        U32 j = lm(ii, sd[1]);
        U32 k = ((0u - h) - lm(g, 0x69010000u)) >> 16;
        U32 m = lm(k, 0x7C932B89u) - j;
        U32 nn = lm(k, 0xF2310000u) - lm(m >> 16, 0x405B6097u);
        nn += lm(j, 0x60970000u);
        acc = nn;
        sum += d + acc;
    }
    out[0] = acc; out[1] = sum;
    return 1;
}

static void base64_8(const U8 b[8], char out[UC_HASH_CCH])
{
    static const char A[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int o = 0;
    for (int i = 0; i < 8; i += 3) {
        U32 v = (U32)b[i] << 16;
        if (i + 1 < 8) v |= (U32)b[i + 1] << 8;
        if (i + 2 < 8) v |= b[i + 2];
        out[o++] = A[(v >> 18) & 63];
        out[o++] = A[(v >> 12) & 63];
        out[o++] = i + 1 < 8 ? A[(v >> 6) & 63] : '=';
        out[o++] = i + 2 < 8 ? A[v & 63] : '=';
    }
    out[o] = 0;
}

/* MD5 + both mixes over `count` 16-bit words, formatted as Base64. */
static int hash_units(const U16 *u, size_t count, int allow_short, char out[UC_HASH_CCH])
{
    out[0] = 0;
    size_t blen = count * 2;
    U8 *bytes = (U8 *)malloc(blen + 4);
    U32 *w = (U32 *)calloc((count + 1) / 2 + 1, sizeof(U32));
    if (!bytes || !w) { free(bytes); free(w); return 0; }
    for (size_t i = 0; i < count; i++) { bytes[2 * i] = (U8)u[i]; bytes[2 * i + 1] = (U8)(u[i] >> 8); }
    for (size_t i = 0; i < (count + 1) / 2; i++)
        w[i] = (U32)u[2 * i] | (2 * i + 1 < count ? (U32)u[2 * i + 1] << 16 : 0u);

    U32 md5[4];
    md5_words(bytes, blen, md5);
    int mix = (int)(blen >> 2) - ((blen & 4) ? 1 : 0);
    U32 pa[2] = { 0, 0 }, pb[2] = { 0, 0 };
    int ok = 1;
    if (mix >= 2) ok = mix_a(w, mix, md5, SEED_A, pa) && mix_b(w, mix, md5, SEED_B, pb);
    else if (!allow_short) ok = 0;
    if (ok) {
        U32 lo = pa[0] ^ pb[0], hi = pa[1] ^ pb[1];
        U8 f[8];
        for (int i = 0; i < 4; i++) { f[i] = (U8)(lo >> (8 * i)); f[4 + i] = (U8)(hi >> (8 * i)); }
        base64_8(f, out);
    }
    free(bytes); free(w);
    return ok;
}

static U16 lower(U16 c) { return (c >= 'A' && c <= 'Z') ? (U16)(c + 32) : c; }

int uc_hash_legacy(const uint16_t *s, size_t n, char out[UC_HASH_CCH])
{
    out[0] = 0;
    U16 *u = (U16 *)malloc((n + 1) * sizeof(U16));
    if (!u) return 0;
    for (size_t i = 0; i < n; i++) u[i] = lower(s[i]);
    u[n] = 0;
    int ok = hash_units(u, n + 1, 0, out);
    free(u);
    return ok;
}

int uc_hash_latest(const uint16_t *s, size_t n, char out[UC_HASH_CCH])
{
    out[0] = 0;
    if (!tables_init()) return 0;
    U16 *low = (U16 *)malloc((n ? n : 1) * sizeof(U16));
    if (!low) return 0;
    for (size_t i = 0; i < n; i++) low[i] = lower(s[i]);
    G.fail = 0;
    size_t np = 0;
    U16 *packed = encode(low, n, &np);
    free(low);
    if (!packed || G.fail) { free(packed); return 0; }
    int ok = hash_units(packed, np, 1, out);
    free(packed);
    return ok;
}

int uc_hash_selftest(void)
{
    static const char kat[] =
        "copyright (c) microsoft. all rights reserved {3822b7ca-c2f4-4889-b8cc-4ce39a8fb81c}"
        ".pdd01dcb06ea49f32a0b4deb148-0249-44c4-a8d3-5409e822c599msedgepdf"
        "s-1-5-21-673349297-2269585490-1023937497-500";
    U16 u[sizeof kat];
    size_t n = sizeof kat - 1;
    for (size_t i = 0; i < n; i++) u[i] = (U16)kat[i];
    char h[UC_HASH_CCH];
    return uc_hash_latest(u, n, h) && strcmp(h, "JOBZ2dl4dKM=") == 0;
}
