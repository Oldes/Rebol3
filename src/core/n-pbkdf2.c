/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  Copyright 2012-2026 Rebol Open Source Contributors
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Module:  n-pbkdf2.c
**  Summary: Fast PBKDF2-HMAC on top of mbedTLS SHA primitives.
**  Section: natives
**  Author:  Oldes
**  Notes:
**      Based on the idea of fastpbkdf2 by Joseph Birr-Pixton (CC0):
**      the HMAC ipad/opad states are computed only once and each iteration
**      costs just 2 raw compression calls (mbedTLS's generic PKCS#5 code
**      does a full HMAC setup per iteration = 4 compressions + overhead).
**
***********************************************************************/

#include "sys-crypt.h"
#include "mbedtls/cipher_wrap.h"
#include "mbedtls/platform.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/platform_util.h"


static void Pbkdf2_Put_BE32(uint32_t n, uint8_t *b)
{
	b[0] = (uint8_t)(n >> 24); b[1] = (uint8_t)(n >> 16);
	b[2] = (uint8_t)(n >>  8); b[3] = (uint8_t)(n);
}

static void Pbkdf2_Put_BE64(uint64_t n, uint8_t *b)
{
	Pbkdf2_Put_BE32((uint32_t)(n >> 32), b);
	Pbkdf2_Put_BE32((uint32_t)n, b + 4);
}

// Note: in mbedtls 3.x `finish` frees (zeroizes) the context, `process` uses only `state`.
// SHA-1 functions have no is224/is384 argument, so these wrappers unify the signatures.
#ifdef MBEDTLS_SHA1_C
#define SHA1_STARTS(c, x)        mbedtls_sha1_starts(c)
#define SHA1_DIGEST(i, n, o, x)  mbedtls_sha1(i, n, o)
#endif

/* Declares a static fast PBKDF2 for one hash family:
**   NAME   - suffix of the generated function
**   CTX    - mbedtls context type
**   WTYPE  - type of the state words, PUT - its big-endian writer
**   BLK    - hash block size, HSZ - digest size (may be truncated: 224/384)
**   X      - is224/is384 flag
*/
#define DECL_FAST_PBKDF2(NAME, CTX, WTYPE, PUT, BLK, HSZ, X,                       \
                         STARTS, UPDATE, FINISH, PROCESS, DIGEST)                 \
static int Fast_Pbkdf2_##NAME(const uint8_t *pw, size_t npw,                      \
	const uint8_t *salt, size_t nsalt, uint32_t iterations,                       \
	uint8_t *out, size_t nout)                                                    \
{                                                                                 \
	CTX ipad, opad, inner, outer;                                                 \
	uint8_t k[BLK], blk[BLK], U[BLK], T[HSZ], cnt[4];                             \
	uint32_t counter, i;                                                          \
	size_t n, offset;                                                             \
	int ret;                                                                      \
                                                                                  \
	/* HMAC key: hash long keys, zero-pad short ones */                           \
	memset(k, 0, BLK);                                                            \
	if (npw > BLK) { if ((ret = DIGEST(pw, npw, k, X)) != 0) goto exit; }         \
	else if (npw > 0) memcpy(k, pw, npw);                                         \
                                                                                  \
	/* precompute inner and outer states (one block each) */                     \
	for (n = 0; n < BLK; n++) blk[n] = k[n] ^ 0x36;                               \
	if ((ret = STARTS(&ipad, X)) != 0 || (ret = UPDATE(&ipad, blk, BLK)) != 0)    \
		goto exit;                                                                \
	for (n = 0; n < BLK; n++) blk[n] = k[n] ^ 0x5C;                               \
	if ((ret = STARTS(&opad, X)) != 0 || (ret = UPDATE(&opad, blk, BLK)) != 0)    \
		goto exit;                                                                \
                                                                                  \
	for (counter = 1, offset = 0; offset < nout; counter++, offset += HSZ) {      \
		Pbkdf2_Put_BE32(counter, cnt);                                            \
		/* U1 = HMAC(P, S || INT_BE32(counter)) */                                \
		inner = ipad; outer = opad;                                               \
		if ((ret = UPDATE(&inner, salt, nsalt)) != 0                              \
			|| (ret = UPDATE(&inner, cnt, 4)) != 0                                \
			|| (ret = FINISH(&inner, U)) != 0                                     \
			|| (ret = UPDATE(&outer, U, HSZ)) != 0                                \
			|| (ret = FINISH(&outer, U)) != 0) goto exit;                         \
		memcpy(T, U, HSZ);                                                        \
                                                                                  \
		/* U holds the previous digest followed by the constant MD padding     */ \
		/* for a (BLK + HSZ) bytes long message, so it is one complete block.  */ \
		memset(U + HSZ, 0, BLK - HSZ);                                            \
		U[HSZ] = 0x80;                                                            \
		Pbkdf2_Put_BE32((uint32_t)((BLK + HSZ) * 8), U + BLK - 4);                \
		inner = ipad; outer = opad; /* only `state` is changed below */           \
                                                                                  \
		/* Uc = HMAC(P, Uc-1); T ^= Uc */                                         \
		for (i = 1; i < iterations; i++) {                                        \
			memcpy(inner.MBEDTLS_PRIVATE(state), ipad.MBEDTLS_PRIVATE(state),     \
				sizeof(ipad.MBEDTLS_PRIVATE(state)));                             \
			if ((ret = PROCESS(&inner, U)) != 0) goto exit;                       \
			for (n = 0; n < HSZ / sizeof(WTYPE); n++)                             \
				PUT(inner.MBEDTLS_PRIVATE(state)[n], U + n * sizeof(WTYPE));      \
			memcpy(outer.MBEDTLS_PRIVATE(state), opad.MBEDTLS_PRIVATE(state),     \
				sizeof(opad.MBEDTLS_PRIVATE(state)));                             \
			if ((ret = PROCESS(&outer, U)) != 0) goto exit;                       \
			for (n = 0; n < HSZ / sizeof(WTYPE); n++)                             \
				PUT(outer.MBEDTLS_PRIVATE(state)[n], U + n * sizeof(WTYPE));      \
			for (n = 0; n < HSZ; n++) T[n] ^= U[n];                               \
		}                                                                         \
		memcpy(out + offset, T, MIN(nout - offset, HSZ));                         \
	}                                                                             \
	ret = 0;                                                                      \
exit:                                                                             \
	mbedtls_platform_zeroize(k, sizeof(k));                                       \
	mbedtls_platform_zeroize(blk, sizeof(blk));                                   \
	mbedtls_platform_zeroize(U, sizeof(U));                                       \
	mbedtls_platform_zeroize(T, sizeof(T));                                       \
	mbedtls_platform_zeroize(&ipad, sizeof(ipad));                                \
	mbedtls_platform_zeroize(&opad, sizeof(opad));                                \
	mbedtls_platform_zeroize(&inner, sizeof(inner));                              \
	mbedtls_platform_zeroize(&outer, sizeof(outer));                              \
	return ret;                                                                   \
}

#ifdef MBEDTLS_SHA1_C
DECL_FAST_PBKDF2(SHA1, mbedtls_sha1_context, uint32_t, Pbkdf2_Put_BE32, 64, 20, 0,
	SHA1_STARTS, mbedtls_sha1_update, mbedtls_sha1_finish,
	mbedtls_internal_sha1_process, SHA1_DIGEST)
#endif
#ifdef MBEDTLS_SHA224_C
DECL_FAST_PBKDF2(SHA224, mbedtls_sha256_context, uint32_t, Pbkdf2_Put_BE32, 64, 28, 1,
	mbedtls_sha256_starts, mbedtls_sha256_update, mbedtls_sha256_finish,
	mbedtls_internal_sha256_process, mbedtls_sha256)
#endif
#ifdef MBEDTLS_SHA256_C
DECL_FAST_PBKDF2(SHA256, mbedtls_sha256_context, uint32_t, Pbkdf2_Put_BE32, 64, 32, 0,
	mbedtls_sha256_starts, mbedtls_sha256_update, mbedtls_sha256_finish,
	mbedtls_internal_sha256_process, mbedtls_sha256)
#endif
#ifdef MBEDTLS_SHA384_C
DECL_FAST_PBKDF2(SHA384, mbedtls_sha512_context, uint64_t, Pbkdf2_Put_BE64, 128, 48, 1,
	mbedtls_sha512_starts, mbedtls_sha512_update, mbedtls_sha512_finish,
	mbedtls_internal_sha512_process, mbedtls_sha512)
#endif
#ifdef MBEDTLS_SHA512_C
DECL_FAST_PBKDF2(SHA512, mbedtls_sha512_context, uint64_t, Pbkdf2_Put_BE64, 128, 64, 0,
	mbedtls_sha512_starts, mbedtls_sha512_update, mbedtls_sha512_finish,
	mbedtls_internal_sha512_process, mbedtls_sha512)
#endif

/* Returns 0 on success, MBEDTLS_ERR_MD_BAD_INPUT_DATA for unsupported digest */
static int Fast_Pbkdf2(mbedtls_md_type_t md_alg, const uint8_t *pw, size_t npw,
	const uint8_t *salt, size_t nsalt, uint32_t iterations, uint8_t *out, size_t nout)
{
	switch (md_alg) {
#ifdef MBEDTLS_SHA1_C
	case MBEDTLS_MD_SHA1:   return Fast_Pbkdf2_SHA1  (pw, npw, salt, nsalt, iterations, out, nout);
#endif
#ifdef MBEDTLS_SHA224_C
	case MBEDTLS_MD_SHA224: return Fast_Pbkdf2_SHA224(pw, npw, salt, nsalt, iterations, out, nout);
#endif
#ifdef MBEDTLS_SHA256_C
	case MBEDTLS_MD_SHA256: return Fast_Pbkdf2_SHA256(pw, npw, salt, nsalt, iterations, out, nout);
#endif
#ifdef MBEDTLS_SHA384_C
	case MBEDTLS_MD_SHA384: return Fast_Pbkdf2_SHA384(pw, npw, salt, nsalt, iterations, out, nout);
#endif
#ifdef MBEDTLS_SHA512_C
	case MBEDTLS_MD_SHA512: return Fast_Pbkdf2_SHA512(pw, npw, salt, nsalt, iterations, out, nout);
#endif
	default: return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
	}
}

/***********************************************************************
**
*/	REBNATIVE(pbkdf2)
/*
//  pbkdf2: native [
//		"Derive a key from a password using PBKDF2 (RFC 8018) with HMAC"
//		pass   [binary! any-string!] "Password (strings are used as UTF-8)"
//		salt   [binary! any-string!] "Salt value"
//		count  [integer!] "Number of iterations"
//		length [integer!] "Requested length of the derived key in bytes"
//		method [word!]    "HMAC digest: sha1 sha224 sha256 sha384 sha512"
//  ]
***********************************************************************/
{
	REBVAL *val_pass   = D_ARG(1);
	REBVAL *val_salt   = D_ARG(2);
	REBVAL *val_count  = D_ARG(3);
	REBVAL *val_length = D_ARG(4);
	REBVAL *val_method = D_ARG(5);

	REBI64  count  = VAL_INT64(val_count);
	REBI64  klen   = VAL_INT64(val_length);
	REBSER *out;
	REBINT  err;
	mbedtls_md_type_t md_alg = Sym_To_MD_Type(VAL_WORD_CANON(val_method));

	switch (md_alg) {
	case MBEDTLS_MD_SHA1:
#ifdef INCLUDE_SHA224
	case MBEDTLS_MD_SHA224:
#endif
	case MBEDTLS_MD_SHA256:
#ifdef INCLUDE_SHA384
	case MBEDTLS_MD_SHA384:
#endif
	case MBEDTLS_MD_SHA512:
		break;
	default: // md5, ripemd160 and unknown words are refused
		Trap_Arg(val_method);
	}
	if (count < 1 || count > 0x7FFFFFFF) Trap_Arg(val_count);
	if (klen  < 1 || klen  > (1 << 20))  Trap_Arg(val_length); // 1 MiB sanity cap

	out = Make_Binary((REBCNT)klen);

	err = Fast_Pbkdf2(
		md_alg, VAL_BIN_AT(val_pass), VAL_BIN_LEN(val_pass), VAL_BIN_AT(val_salt), VAL_BIN_LEN(val_salt),
		(uint32_t)count, BIN_DATA(out), (size_t)klen);
	if (err) {
		CLEAR(BIN_DATA(out), (REBCNT)klen);
		Free_Series(out);
		if (err == MBEDTLS_ERR_MD_BAD_INPUT_DATA) Trap_Arg(val_method); // digest not in this build
		return R_NONE;
	}
	SET_BINARY(D_RET, out);
	BIN_LEN(out) = (REBCNT)klen;
	return R_RET;
}
