#!/usr/bin/env python3
"""Reference implementation of the two Windows "UserChoice" hashes (stdlib only).

This exists to cross-check src/uchash.c; it is not used by the program. Two independent
sources are transcribed here on purpose, so a typo in one of them cannot hide:

  legacy_hash()  the original PS-SFTA PowerShell algorithm (MIT, Danysys / LMongrain), which
                 has been in use on Windows 10 since 2020.
  latest_hash()  the UserChoiceLatest pipeline from cssxn/UserChoiceLatestHash (MIT).

Usage:
  ref_hash.py latest "<canonical input>"
  ref_hash.py legacy "<base string>"
  ref_hash.py vectors            print the test vectors used by tests/uchash_test.c
"""
import base64, hashlib, random, re, struct, sys, os

M32 = 0xFFFFFFFF
M64 = (1 << 64) - 1

# ---------------------------------------------------------------- shared helpers
def ascii_lower(s):
    return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in s)

def u16(s):
    return [ord(c) for c in s]            # tests only use the BMP

def b64(low, high):
    return base64.b64encode(struct.pack("<II", low & M32, high & M32)).decode()

# --------------------------------------------------------------- legacy (PS-SFTA)
def _shr16(x):                            # PowerShell Get-ShiftRight(x, 16) on an int32 == logical shift
    return (x & M32) >> 16

def legacy_hash(base):
    units = u16(base) + [0]
    data = b"".join(struct.pack("<H", u) for u in units)
    md5 = hashlib.md5(data).digest()
    m0, m1 = struct.unpack("<II", md5[:8])
    length_base = len(data)
    length = (1 if (length_base & 4) == 0 else 0) + (length_base >> 2) - 1
    if length <= 1:
        return ""
    dwords = [struct.unpack_from("<I", data, i)[0] for i in range(0, len(data) - 3, 4)]
    loops = ((length - 2) >> 1) + 1

    # pass 1
    md51 = ((m0 | 1) + 0x69FB0000) & M32
    md52 = ((m1 | 1) + 0x13DB0000) & M32
    cache = out1 = 0
    p = 0
    for _ in range(loops):
        r0 = (dwords[p] + out1) & M32
        r1 = dwords[p + 1]
        p += 2
        r2a = (r0 * md51 - 0x10FA9605 * _shr16(r0)) & M32
        r2b = (0x79F8A395 * r2a + 0x689B6B9F * _shr16(r2a)) & M32
        r3 = (0xEA970001 * r2b - 0x3C101569 * _shr16(r2b)) & M32
        r4 = (r3 + r1) & M32
        r5 = (cache + r3) & M32
        r6a = (r4 * md52 - 0x3CE8EC25 * _shr16(r4)) & M32
        r6b = (0x59C3AF2D * r6a - 0x2232E0F1 * _shr16(r6a)) & M32
        out1 = (0x1EC90001 * r6b + 0x35BD1EC9 * _shr16(r6b)) & M32
        out2 = (r5 + out1) & M32
        cache = out2
    a1, a2 = out1, out2

    # pass 2
    md51 = (m0 | 1) & M32
    md52 = (m1 | 1) & M32
    cache = out1 = 0
    p = 0
    for _ in range(loops):
        r0 = (dwords[p] + out1) & M32
        p += 2
        r1a = (r0 * md51) & M32
        r1b = (0xB1110000 * r1a - 0x30674EEF * _shr16(r1a)) & M32
        r2a = (0x5B9F0000 * r1b - 0x78F7A461 * _shr16(r1b)) & M32
        r2b = (0x12CEB96D * _shr16(r2a) - 0x46930000 * r2a) & M32
        r3 = (0x1D830000 * r2b + 0x257E1D83 * _shr16(r2b)) & M32
        r4a = (md52 * ((r3 + dwords[p - 1]) & M32)) & M32
        r4b = (0x16F50000 * r4a - 0x5D8BE90B * _shr16(r4a)) & M32
        r5a = (0x96FF0000 * r4b - 0x2C7C6901 * _shr16(r4b)) & M32
        r5b = (0x2B890000 * r5a + 0x7C932B89 * _shr16(r5a)) & M32
        out1 = (0x9F690000 * r5b - 0x405B6097 * _shr16(r5b)) & M32
        out2 = (out1 + cache + r3) & M32
        cache = out2
    b1, b2 = out1, out2
    return b64(a1 ^ b1, a2 ^ b2)

# --------------------------------------------------------- latest (cssxn pipeline)
def _load_tables():
    here = os.path.dirname(os.path.abspath(__file__))
    src = open(os.path.join(here, "..", "src", "uchash_tables.inc")).read()
    t = {}
    for m in re.finditer(r'static const char \*(\w+) =\s*((?:"[0-9a-fA-F]*"\s*)+);', src):
        t[m.group(1)] = bytes.fromhex("".join(re.findall(r'"([0-9a-fA-F]*)"', m.group(2))))
    return t

T = _load_tables()
TOKEN_REGION = T["kTokenRegionHex"]
LONG_REWIND = T["kLongRewindTableHex"]
FALLBACK = T["kFallbackTokenCodeTableHex"]
BYTE_CLASS = T["kByteClassTableHex"]
BITW = T["kTokenBitWidthTableHex"]
SHORT_REWIND = T["kShortRewindTableHex"]

SENTINEL = 0xFBFFFFFFFFFFFFFF
RA, RB, RC = 0xFE00000000000000, 0xFE00800000000000, 0xFF00000000000000
RD, RE = 0xFE007DFFFFFFFFFF, 0xFE00801F7FFFFFFF
SEEN_MOD = 0x2803
SEED_A = [0x69FB0000, 0x79F8A395, 0x3CE8EC25, 0x10FA9605]
SEED_B = [0x12CEB96D, 0x2B890000, 0x5B9F0000, 0x2C7C6901]


class Enc:
    def __init__(self):
        self.entries = [0]
        self.seen = [0] * 161
        self.last = SENTINEL
        self.repeat = 0
        self.total = 0

    @property
    def count(self):
        return len(self.entries)

    def test_seen(self, v):
        i = v % SEEN_MOD
        return (self.seen[i >> 6] >> (i & 63)) & 1 == 1

    def mark(self, v):
        i = v % SEEN_MOD
        self.seen[i >> 6] |= 1 << (i & 63)

    def push(self, v):
        self.entries.append(v & M64)

    def find_prev(self, v):
        if not self.test_seen(v) or self.count == 0:
            return None
        start = self.count - 0x400 if self.count > 0x400 else 0
        idx = self.count - 1
        while True:
            if self.entries[idx] == v:
                return self.count - idx
            if idx == start:
                break
            idx -= 1
        return None


def cls(ch):
    hi = ch >> 8
    return (BYTE_CLASS[hi] + 8) & 0xFF if hi else BYTE_CLASS[ch & 0xFF]


def at(inp, i, end):
    return inp[i] if i < end else 0


def build_token2(inp, cur, end):
    if cur >= end:
        return None
    remain = end - cur
    code = TOKEN_REGION[cls(inp[cur]) + 256]
    tl = TOKEN_REGION[code]
    if tl > remain:
        code = FALLBACK[remain]
        tl = TOKEN_REGION[code]
    base = 168 * code
    while True:
        pos = 0
        retry = False
        if tl != 0:
            probe = cur
            while probe < end:
                if inp[probe] >= (1 << BITW[base + pos]):
                    code += 1
                    base = 168 * code
                    tl = TOKEN_REGION[code]
                    if tl <= remain:
                        retry = True
                    break
                pos += 1
                probe += 1
                if pos >= tl:
                    break
        if not retry:
            break
    tok = (code & 0xFF) << 56
    shift = 56
    for i in range(tl & 0xFF):
        if cur + i >= end:
            break
        shift -= BITW[base + i]
        tok |= inp[cur + i] << shift
    return tok & M64, code & 0xFF, tl & 0xFF


def emit(st, cur, tok, kind):
    """Returns the new cursor."""
    if tok == st.last and st.repeat < 0xFF:
        st.repeat += 1
        return cur
    if st.repeat != 0:
        d = st.find_prev(st.last)
        if d is not None:
            rm1 = st.repeat - 1
            if d >= 0x800:
                packed = ((tok >> 17) | (rm1 << 48) | RA) & M64
                st.mark(packed)
                st.push(packed)
                entry = st.last
            else:
                entry = ((tok >> 27) | ((((d - 1) & 0x3FF) | (rm1 << 11)) << 37) | RB) & M64
            st.mark(entry)
            st.push(entry)
            st.last = SENTINEL
            st.repeat = 0
            return cur - (LONG_REWIND[kind] if d >= 0x800 else SHORT_REWIND[kind])
    st.mark(tok)
    st.push(tok)
    st.last = tok
    return cur


def flush(st):
    if st.repeat == 0:
        return
    if st.last == SENTINEL:
        entry = ((st.repeat << 48) | RD) & M64
    else:
        d = st.find_prev(st.last)
        if d is not None and d != 0:
            entry = ((((d - 1) & 0x3FF) | ((st.repeat - 1) << 11)) << 37 | RE) & M64
        else:
            rt = ((st.repeat << 48) | RD) & M64
            st.mark(rt)
            st.push(rt)
            entry = st.last
    st.mark(entry)
    st.push(entry)
    st.repeat = 0
    st.last = SENTINEL


MAX_CCH = 330   # the first pass Windows runs on longer inputs is not implemented


def encode(lowered):
    inp = u16(lowered)
    n = len(inp)
    if n > MAX_CCH:
        raise ValueError("input too long")
    st = Enc()
    cur = 0
    steps = 0
    while cur < n:
        tok, kind, tl = build_token2(inp, cur, n)
        st.total += tl
        cur += tl
        cur = emit(st, cur, tok, kind)
        steps += 1
        if steps > 100000 or cur < 0:
            raise RuntimeError("runaway")
    flush(st)
    st.entries[0] = (st.total | (st.count << 32)) & M64
    packed = []
    for v in st.entries:
        for sh in (0, 16, 32, 48):
            part = (v >> sh) & 0xFFFF
            if part:
                packed.append(part)
    packed.append(0)
    return packed


def lm(a, b):
    return (a * b) & M32


def mix_a(w, count, md5, seed):
    md0 = (md5[0] | 1) & M32
    md1 = ((md5[1] | 1) + 0x13DB0000) & M32
    s0, s1, s2, s3 = seed
    total = acc = 0
    mul = (md0 + s0) & M32
    for i in range(((count - 2) >> 1) + 1):
        acc = (acc + w[2 * i]) & M32
        x = acc
        t = (lm(x, mul) - lm(x >> 16, s3)) & M32
        s = (lm(t, s1) + lm(t >> 16, 0x689B6B9F)) & M32
        part1 = (lm(s, 0xEA970001) - lm(s >> 16, 0x3C101569)) & M32
        wv = (w[2 * i + 1] + part1) & M32
        t2 = (lm(wv, md1) - lm(wv >> 16, s2)) & M32
        temp = (lm(t2, 0x59C3AF2D) - lm(t2 >> 16, 0x2232E0F1)) & M32
        acc = (lm(temp >> 16, 0x35BD1EC9) + lm(temp, 0x1EC90001)) & M32
        total = (total + part1 + acc) & M32
    return acc, total


def mix_b(w, count, md5, seed):
    s0, s1, s2, s3 = seed
    md1 = (md5[1] | 1) & M32
    md0 = (md5[0] | 1) & M32
    total = acc = 0
    for i in range(((count - 2) >> 1) + 1):
        x = (w[2 * i] + acc) & M32
        t = lm(x, md0)
        a = (lm(t, 0xB1110000) - lm(t >> 16, 0x30674EEF)) & M32
        b = lm(a, s2)
        c = (b - lm(a >> 16, 0x78F7A461)) & M32
        c = (lm(c >> 16, s0) - lm(a >> 16, 0x164D0000) - lm(b, 0x46930000)) & M32
        d = (lm(c >> 16, 0x257E1D83) + lm(c, 0x1D830000)) & M32
        e = (w[2 * i + 1] + d) & M32
        f = lm(e, md1)
        g = (lm(f, 0x16F50000) - lm(f >> 16, 0x5D8BE90B)) & M32
        h = lm(g >> 16, s3)
        ii = (lm(g, 0x69010000) + h) & M32
        j = lm(ii, s1)
        k = (((0 - h) & M32) - lm(g, 0x69010000)) & M32
        k >>= 16
        m = (lm(k, 0x7C932B89) - j) & M32
        n = (lm(k, 0xF2310000) - lm(m >> 16, 0x405B6097)) & M32
        n = (n + lm(j, 0x60970000)) & M32
        acc = n
        total = (total + d + acc) & M32
    return acc, total


def hash_packed(units):
    data = b"".join(struct.pack("<H", u) for u in units)
    md5 = struct.unpack("<4I", hashlib.md5(data).digest())
    blen = len(data)
    mix = blen >> 2 if (blen & 4) == 0 else (blen >> 2) - 1
    words = []
    for i in range((len(units) + 1) // 2):
        lo = units[2 * i]
        hi = units[2 * i + 1] if 2 * i + 1 < len(units) else 0
        words.append(lo | (hi << 16))
    pa = pb = (0, 0)
    if mix >= 2:
        pa = mix_a(words, mix, md5, SEED_A)
        pb = mix_b(words, mix, md5, SEED_B)
    return b64(pa[0] ^ pb[0], pa[1] ^ pb[1])


def latest_hash(canonical):
    return hash_packed(encode(ascii_lower(canonical)))


def legacy_hash_via_mix(base):
    """Legacy hash through the shared mixers (as the C code does it); must equal legacy_hash()."""
    return hash_packed(u16(ascii_lower(base)) + [0])


KAT_INPUT = ("copyright (c) microsoft. all rights reserved {3822b7ca-c2f4-4889-b8cc-4ce39a8fb81c}"
             ".pdd01dcb06ea49f32a0b4deb148-0249-44c4-a8d3-5409e822c599msedgepdf"
             "s-1-5-21-673349297-2269585490-1023937497-500")
KAT_HASH = "JOBZ2dl4dKM="


def vectors():
    rnd = random.Random(20261022)
    alphabet = "abcdefghijklmnopqrstuvwxyz0123456789-.{}_ "
    out = []
    # realistic legacy and latest inputs
    sids = ["s-1-5-21-673349297-2269585490-1023937497-1001", "s-1-12-1-1234567890-987654321-1122334455-5566778899"]
    exps = "user choice set via windows user experience {d18b6dd5-6124-4341-9318-804003bafa0b}"
    progs = ["vlc.mp4", "appxd4nrz8ff68srnhf9t5a8sbjyar1cr723", "applications\\notepad++.exe",
             "fileassocmanager.irfanview.1a2b3c4d", "msedgepdf", "a"]
    for i in range(24):
        ext = ["." + "".join(rnd.choice("abcdefghijklmnopqrstuvwxyz0123456789") for _ in range(rnd.randint(2, 6)))
               for _ in range(1)][0]
        sid = rnd.choice(sids)
        prog = rnd.choice(progs)
        ts = "%016x" % ((0x01DC0000 << 32) | rnd.getrandbits(32) // 10000 * 10000)
        out.append(("legacy", ext + sid + prog + ts[:12] + "00000000"[:4] + exps))
    for i in range(12):
        n = rnd.randint(1, 60)
        out.append(("legacy", "".join(rnd.choice(alphabet) for _ in range(n))))
    guid = "{3822b7ca-c2f4-4889-b8cc-4ce39a8fb81c}"
    for i in range(24):
        sid = rnd.choice(sids)
        prog = rnd.choice(progs)
        ext = "." + "".join(rnd.choice("abcdefghijklmnopqrstuvwxyz") for _ in range(rnd.randint(2, 5)))
        mid = "-".join("%0*x" % (w, rnd.getrandbits(w * 4)) for w in (8, 4, 4, 4, 12))
        ts = "%016x" % (0x01DC000000000000 | (rnd.getrandbits(40) // 10000 * 10000))
        out.append(("latest", "copyright (c) microsoft. all rights reserved " + guid + ext + ts + mid + prog + sid))
    # stress: very short, maximum length and highly repetitive inputs (exercise run/back-reference coding)
    out.append(("latest", "a"))
    out.append(("latest", "ab"))
    out.append(("latest", "x" * 40))
    out.append(("latest", "abcabcabc" * 20))
    out.append(("latest", "".join(rnd.choice(alphabet) for _ in range(330))))
    out.append(("latest", "".join(rnd.choice("ab") for _ in range(330))))
    out.append(("latest", "ab" * 165))
    for _ in range(40):
        out.append(("latest", "".join(rnd.choice("abc0123 .") for _ in range(rnd.randint(3, 330)))))
    return out


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "vectors"
    if cmd == "latest":
        print(latest_hash(sys.argv[2]))
    elif cmd == "legacy":
        print(legacy_hash(ascii_lower(sys.argv[2])))
    else:
        assert latest_hash(KAT_INPUT) == KAT_HASH, "KAT failed: %s" % latest_hash(KAT_INPUT)
        bad = 0
        for kind, s in vectors():
            if kind == "legacy":
                a, b = legacy_hash(ascii_lower(s)), legacy_hash_via_mix(s)
                if a != b:
                    bad += 1
                    print("LEGACY MIXER MISMATCH", repr(s), a, b)
        print("KAT ok; legacy PS-vs-mixer mismatches:", bad)
