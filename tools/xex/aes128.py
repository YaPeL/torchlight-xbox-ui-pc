"""AES-128 CBC decryption in plain Python (FIPS-197), for xex_dump.py when pycryptodome is not
installed: the installer then needs nothing beyond Python. Decryption only, table driven."""
import struct


def _xtime(a):
    return ((a << 1) ^ 0x1B) & 0xFF if a & 0x80 else a << 1


def _mul(a, b):
    r = 0
    while b:
        if b & 1:
            r ^= a
        a, b = _xtime(a), b >> 1
    return r


def _sbox():
    s = [0] * 256
    for x in range(256):
        inv = 0 if x == 0 else next(y for y in range(1, 256) if _mul(x, y) == 1)
        b, r = inv, inv
        for _ in range(4):
            b = ((b << 1) | (b >> 7)) & 0xFF
            r ^= b
        s[x] = r ^ 0x63
    return s


SBOX = _sbox()
INV_SBOX = [0] * 256
for _i, _v in enumerate(SBOX):
    INV_SBOX[_v] = _i
# The inverse round's tables: InvSubBytes then InvMixColumns of one byte, per row.
_T = [[0] * 256 for _ in range(4)]
for _x in range(256):
    _s = INV_SBOX[_x]
    _w = (_mul(_s, 14) << 24) | (_mul(_s, 9) << 16) | (_mul(_s, 13) << 8) | _mul(_s, 11)
    for _r in range(4):
        _T[_r][_x] = ((_w >> (8 * _r)) | (_w << (32 - 8 * _r))) & 0xFFFFFFFF
T0, T1, T2, T3 = _T


def _expand(key):
    w = list(struct.unpack(">4I", key))
    rcon = 1
    for i in range(4, 44):
        t = w[i - 1]
        if i % 4 == 0:
            t = ((t << 8) | (t >> 24)) & 0xFFFFFFFF
            t = (SBOX[t >> 24] << 24) | (SBOX[(t >> 16) & 0xFF] << 16) | (SBOX[(t >> 8) & 0xFF] << 8) | SBOX[t & 0xFF]
            t ^= rcon << 24
            rcon = _xtime(rcon)
        w.append(w[i - 4] ^ t)
    return w


def _inv_mix(word):
    """InvMixColumns of a round key word (the equivalent inverse cipher's keys)."""
    return (T0[SBOX[word >> 24]] ^ T1[SBOX[(word >> 16) & 0xFF]] ^ T2[SBOX[(word >> 8) & 0xFF]]
            ^ T3[SBOX[word & 0xFF]])


class Decryptor:
    def __init__(self, key):
        if len(key) != 16:
            raise ValueError("AES-128 needs a 16 byte key")
        w = _expand(key)
        rounds = [w[4 * r:4 * r + 4] for r in range(11)]
        self.keys = [rounds[10]] + [[_inv_mix(x) for x in rounds[r]] for r in range(9, 0, -1)] + [rounds[0]]

    def block(self, s0, s1, s2, s3):
        k = self.keys
        s0 ^= k[0][0]; s1 ^= k[0][1]; s2 ^= k[0][2]; s3 ^= k[0][3]
        for r in range(1, 10):
            kr = k[r]
            s0, s1, s2, s3 = (
                T0[s0 >> 24] ^ T1[(s3 >> 16) & 0xFF] ^ T2[(s2 >> 8) & 0xFF] ^ T3[s1 & 0xFF] ^ kr[0],
                T0[s1 >> 24] ^ T1[(s0 >> 16) & 0xFF] ^ T2[(s3 >> 8) & 0xFF] ^ T3[s2 & 0xFF] ^ kr[1],
                T0[s2 >> 24] ^ T1[(s1 >> 16) & 0xFF] ^ T2[(s0 >> 8) & 0xFF] ^ T3[s3 & 0xFF] ^ kr[2],
                T0[s3 >> 24] ^ T1[(s2 >> 16) & 0xFF] ^ T2[(s1 >> 8) & 0xFF] ^ T3[s0 & 0xFF] ^ kr[3])
        kr, S = k[10], INV_SBOX
        return (
            ((S[s0 >> 24] << 24) | (S[(s3 >> 16) & 0xFF] << 16) | (S[(s2 >> 8) & 0xFF] << 8) | S[s1 & 0xFF]) ^ kr[0],
            ((S[s1 >> 24] << 24) | (S[(s0 >> 16) & 0xFF] << 16) | (S[(s3 >> 8) & 0xFF] << 8) | S[s2 & 0xFF]) ^ kr[1],
            ((S[s2 >> 24] << 24) | (S[(s1 >> 16) & 0xFF] << 16) | (S[(s0 >> 8) & 0xFF] << 8) | S[s3 & 0xFF]) ^ kr[2],
            ((S[s3 >> 24] << 24) | (S[(s2 >> 16) & 0xFF] << 16) | (S[(s1 >> 8) & 0xFF] << 8) | S[s0 & 0xFF]) ^ kr[3])


def cbc_decrypt(key, iv, data):
    if len(data) % 16:
        raise ValueError("CBC data must be whole 16 byte blocks")
    d = Decryptor(key)
    words = struct.unpack(f">{len(data) // 4}I", data)
    out = [0] * len(words)
    p0, p1, p2, p3 = struct.unpack(">4I", iv)
    for i in range(0, len(words), 4):
        c0, c1, c2, c3 = words[i:i + 4]
        o0, o1, o2, o3 = d.block(c0, c1, c2, c3)
        out[i:i + 4] = (o0 ^ p0, o1 ^ p1, o2 ^ p2, o3 ^ p3)
        p0, p1, p2, p3 = c0, c1, c2, c3
    return struct.pack(f">{len(out)}I", *out)
