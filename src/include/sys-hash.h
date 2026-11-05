//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#define MURMUR_HASH_3_X86_32_C1     0xcc9e2d51
#define MURMUR_HASH_3_X86_32_C2     0x1b873593

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here
FORCE_INLINE REBCNT getblock32(const REBCNT* p, int i) {
    // binary data may not be 4-byte aligned, so avoid a direct (unaligned) read;
    // compilers turn this memcpy into a single load where it is allowed
    REBCNT k;
    memcpy(&k, (const REBYTE*)p + (i * 4), 4);
    return k;
}
/*-----------------------------------------------------------------------------
// Block mix - mix the key block, combine with hash block, mix the hash block,
// repeat. */

FORCE_INLINE void bmix(REBCNT* h1, REBCNT* k1)
{
    *k1 *= MURMUR_HASH_3_X86_32_C1;
    *k1 = ROTL32(*k1, 15);
    *k1 *= MURMUR_HASH_3_X86_32_C2;
    *h1 ^= *k1;
    *h1 = ROTL32(*h1, 13); *h1 = *h1 * 5 + 0xe6546b64;
}
//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche
FORCE_INLINE REBCNT fmix32(REBCNT h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

//-----------------------------------------------------------------------------
// Quadratic probing - the same sequence as Hash_Probe: (hash + i + 3*i*i) % len,
// but computed incrementally without divisions (the step between probes is 6*i+4).
// Hash tables are always at least 251 slots long, so a single subtraction is enough.
#define PROBE_START(slot, step, hash, len) ((slot) = (hash) % (len), (step) = 4 % (len))
#define PROBE_NEXT(slot, step, len) ( \
    (slot) += (step), (slot) -= ((slot) >= (len)) ? (len) : 0, \
    (step) += 6,      (step) -= ((step) >= (len)) ? (len) : 0)