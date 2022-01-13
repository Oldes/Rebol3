/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  Copyright 2012-2021 Rebol Open Source Contributors
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
**  Module:  n-crypt.c
**  Summary: native functions for data sets
**  Section: natives
**  Author:  Oldes, Cyphre
**  Notes:
**
***********************************************************************/

#include "sys-core.h"
#include "sys-rc4.h"
#include "sys-aes.h"
#include "uECC.h"
#ifndef EXCLUDE_CHACHA20POLY1305
#include "sys-chacha20.h"
#include "sys-poly1305.h"
#endif
#include "mbedtls/rsa.h"
#include "mbedtls/dhm.h"
#include "mbedtls/bignum.h"
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;

const struct uECC_Curve_t* ECC_curves[5] = {0,0,0,0,0};
typedef struct {
	REBCNT  curve_type;
	uint8_t public[64];
	uint8_t private[32];
} ECC_CTX;

typedef mbedtls_rsa_context RSA_CTX;
typedef mbedtls_dhm_context DHM_CTX;

// these 2 functions were defined as static in dhm.c file, so are not in the header!
extern int dhm_check_range(const mbedtls_mpi *param, const mbedtls_mpi *P);
extern int dhm_random_below(mbedtls_mpi *R, const mbedtls_mpi *M,
	int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);

/***********************************************************************
**
*/	void Init_Crypt(void)
/*
***********************************************************************/
{
	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);
	const char *pers = "rebol";

	mbedtls_ctr_drbg_seed(
		&ctr_drbg, mbedtls_entropy_func, &entropy,
		(const unsigned char *)pers, strlen(pers));

	Register_Handle(SYM_AES,  sizeof(AES_CTX), NULL);
	Register_Handle(SYM_ECDH, sizeof(ECC_CTX), NULL);
	Register_Handle(SYM_RC4,  sizeof(RC4_CTX), NULL);
	Register_Handle(SYM_DHM,  sizeof(DHM_CTX), (REB_HANDLE_FREE_FUNC)mbedtls_dhm_free);
	Register_Handle(SYM_RSA,  sizeof(RSA_CTX), (REB_HANDLE_FREE_FUNC)mbedtls_rsa_free);
#ifndef EXCLUDE_CHACHA20POLY1305
	Register_Handle(SYM_CHACHA20, sizeof(poly1305_context), NULL);
	Register_Handle(SYM_POLY1305, sizeof(poly1305_context), NULL);
	Register_Handle(SYM_CHACHA20POLY1305, sizeof(chacha20poly1305_ctx), NULL);
#endif
}

/***********************************************************************
**
*/	REBNATIVE(rc4)
/*
//	rc4: native [
//		"Encrypt/decrypt data (modifies) using RC4 algorithm."
//
//		/key "Provided only for the first time to get stream HANDLE!"
//			crypt-key [binary!]  "Crypt key."
//		/stream
//			ctx  [handle!] "Stream cipher context."
//			data [binary!] "Data to encrypt/decrypt."
//	]
***********************************************************************/
{
    REBOOL  ref_key       = D_REF(1);
    REBVAL *val_crypt_key = D_ARG(2); 
    REBOOL  ref_stream    = D_REF(3);
    REBVAL *val_ctx       = D_ARG(4);
    REBVAL *val_data      = D_ARG(5);

    REBVAL *ret = D_RET;

    if(ref_stream) {
    	if (NOT_VALID_CONTEXT_HANDLE(val_ctx, SYM_RC4)) Trap0(RE_INVALID_HANDLE);

    	REBYTE *data = VAL_BIN_AT(val_data);
    	RC4_crypt((RC4_CTX*)VAL_HANDLE_CONTEXT_DATA(val_ctx), data, data, VAL_LEN(val_data));
    	DS_RET_VALUE(val_data);

    } else if (ref_key) {
    	//key defined - setup new context
		MAKE_HANDLE(ret, SYM_RC4);
		RC4_setup(
			(RC4_CTX*)VAL_HANDLE_CONTEXT_DATA(ret),
            VAL_BIN_AT(val_crypt_key),
            VAL_LEN(val_crypt_key)
        );
    }
    return R_RET;
}


/***********************************************************************
**
*/	REBNATIVE(aes)
/*
//  aes: native [
//		"Encrypt/decrypt data using AES algorithm. Returns stream cipher context handle or encrypted/decrypted data."
//		/key                "Provided only for the first time to get stream HANDLE!"
//			crypt-key [binary!] "Crypt key (16 or 32 bytes)."
//			iv  [none! binary!] "Optional initialization vector (16 bytes)."
//		/decrypt            "Use the crypt-key for decryption (default is to encrypt)"
//		/stream
//			ctx [handle!]   "Stream cipher context."
//			data [binary!]  "Data to encrypt/decrypt."
//  ]
***********************************************************************/
{
	REBOOL  ref_key       = D_REF(1);
    REBVAL *val_crypt_key = D_ARG(2);
    REBVAL *val_iv        = D_ARG(3);
	REBOOL  ref_decrypt   = D_REF(4);
    REBOOL  ref_stream    = D_REF(5);
    REBVAL *val_ctx       = D_ARG(6);
    REBVAL *val_data      = D_ARG(7);
    
    REBVAL *ret = D_RET;
	REBINT  len, pad_len;

	//TODO: could be optimized by reusing the handle

	if (ref_key) {
    	//key defined - setup new context

    	uint8_t iv[AES_IV_SIZE];

		if (IS_BINARY(val_iv)) {
			if (VAL_LEN(val_iv) < AES_IV_SIZE) {
				return R_NONE;
			}
			memcpy(iv, VAL_BIN_AT(val_iv), AES_IV_SIZE);
		} else {
			//TODO: Use ECB encryption if IV is not specified
			memset(iv, 0, AES_IV_SIZE);
		}
		
		len = VAL_LEN(val_crypt_key) << 3;

		if (len != 128 && len != 256) {
			return R_NONE;
		}

		MAKE_HANDLE(ret, SYM_AES);

		AES_set_key(
			(AES_CTX*)VAL_HANDLE_CONTEXT_DATA(ret),
			VAL_BIN_AT(val_crypt_key),
			(const uint8_t *)iv,
			(len == 128) ? AES_MODE_128 : AES_MODE_256
		);

		if (ref_decrypt) AES_convert_key((AES_CTX*)VAL_HANDLE_CONTEXT_DATA(ret));

    } else if(ref_stream) {

    	if (NOT_VALID_CONTEXT_HANDLE(val_ctx, SYM_AES)) {
			Trap0(RE_INVALID_HANDLE);
			return R_NONE;
		}		
		AES_CTX *aes_ctx = (AES_CTX *)VAL_HANDLE_CONTEXT_DATA(val_ctx);

    	len = VAL_LEN(val_data);
    	if (len == 0) return R_NONE;

		pad_len = (((len - 1) >> 4) << 4) + AES_BLOCKSIZE;

		REBYTE *data = VAL_BIN_AT(val_data);
		REBYTE *pad_data;

		if (len < pad_len) {
			//  make new data input with zero-padding
			//TODO: instead of making new data, the original could be extended with padding.
			pad_data = (REBYTE*)MAKE_MEM(pad_len);
			memset(pad_data, 0, pad_len);
			memcpy(pad_data, data, len);
			data = pad_data;
		}
		else {
			pad_data = NULL;
		}

		REBSER  *binaryOut = Make_Binary(pad_len);

		if (aes_ctx->key_mode == AES_MODE_DECRYPT) {
			AES_cbc_decrypt(aes_ctx, data, BIN_HEAD(binaryOut),	pad_len);
		}
		else {
			AES_cbc_encrypt(aes_ctx, data, BIN_HEAD(binaryOut),	pad_len);
		}
		if (pad_data) FREE_MEM(pad_data);

		SET_BINARY(ret, binaryOut);
		VAL_TAIL(ret) = pad_len;

    }
	return R_RET;
}

/***********************************************************************
**
*/	REBNATIVE(rsa_init)
/*
//  rsa-init: native [
//		"Creates a context which is than used to encrypt or decrypt data using RSA"
//		n  [binary!]  "Modulus"
//		e  [binary!]  "Public exponent"
//		/private "Init also private values"
//			d [binary!] "Private exponent"
//			p [binary!] "Prime number 1"
//			q [binary!] "Prime number 2"
//  ]
***********************************************************************/
{
	REBSER *n       = VAL_SERIES(D_ARG(1));
	REBSER *e       = VAL_SERIES(D_ARG(2));
	REBOOL  ref_private =        D_REF(3);
	REBSER *d       = VAL_SERIES(D_ARG(4));
	REBSER *p       = VAL_SERIES(D_ARG(5));
	REBSER *q       = VAL_SERIES(D_ARG(6));

	int err = 0;
	REBVAL *ret = D_RET;
	RSA_CTX *rsa_ctx;

	MAKE_HANDLE(ret, SYM_RSA);
	rsa_ctx = (RSA_CTX*)VAL_HANDLE_CONTEXT_DATA(ret);

	mbedtls_rsa_init(rsa_ctx);

	if (ref_private) {
		err = mbedtls_rsa_import_raw(
			rsa_ctx,
			BIN_DATA(n), BIN_LEN(n),
			BIN_DATA(p), BIN_LEN(p),
			BIN_DATA(q), BIN_LEN(q),
			BIN_DATA(d), BIN_LEN(d),
			BIN_DATA(e), BIN_LEN(e)
		);
		if (err != 0 
			|| mbedtls_rsa_complete(rsa_ctx) != 0
			|| mbedtls_rsa_check_privkey(rsa_ctx) != 0 
		) return R_NONE;
	} else {
		err = mbedtls_rsa_import_raw(
			rsa_ctx,
			BIN_DATA(n), BIN_LEN(n),
			NULL, 0,
			NULL, 0,
			NULL, 0,
			BIN_DATA(e), BIN_LEN(e)
		);
		if (err != 0 
			|| mbedtls_rsa_complete(rsa_ctx) != 0
			|| mbedtls_rsa_check_pubkey(rsa_ctx) != 0 
		) return R_NONE;
	}
	return R_RET;
}
#ifdef unused 
static int myrand(void *rng_state, unsigned char *output, size_t len)
{
#if !defined(__OpenBSD__) && !defined(__NetBSD__)
	size_t i;

	if (rng_state != NULL)
		rng_state = NULL;

	for (i = 0; i < len; ++i)
		output[i] = rand();
#else
	if (rng_state != NULL)
		rng_state = NULL;

	arc4random_buf(output, len);
#endif /* !OpenBSD && !NetBSD */

	return(0);
}
#endif

/***********************************************************************
**
*/	REBNATIVE(rsa)
/*
//  rsa: native [
//		"Encrypt/decrypt/sign/verify data using RSA cryptosystem. Only one refinement must be used!"
//		rsa-key [handle!] "RSA context created using `rsa-init` function"
//		data    [binary! none!] "Data to work with. Use NONE to release the RSA handle resources!"
//		/encrypt  "Use public key to encrypt data"
//		/decrypt  "Use private key to decrypt data"
//		/sign     "Use private key to sign data"
//		/verify   "Use public key to verify signed data (returns TRUE or FALSE)"
//		 signature [binary!] "Result of the sign call"
//  ]
***********************************************************************/
{
	REBVAL *key         = D_ARG(1);
	REBVAL *val_data    = D_ARG(2);
	REBOOL  refEncrypt  = D_REF(3);
	REBOOL  refDecrypt  = D_REF(4);
	REBOOL  refSign     = D_REF(5);
	REBOOL  refVerify   = D_REF(6);
	REBVAL *val_sign    = D_ARG(7);

	RSA_CTX *rsa;
	REBSER  *data;
	REBYTE  *inBinary;
	REBYTE  *outBinary;
	REBYTE   hash[32];
	REBINT   inBytes;
	REBINT   outBytes;
	REBINT   err = 0;

	// make sure that only one refinement is used!
	if(
		(refEncrypt && (refDecrypt || refSign    || refVerify)) ||
		(refDecrypt && (refEncrypt || refSign    || refVerify)) ||
		(refSign    && (refDecrypt || refEncrypt || refVerify)) ||
		(refVerify  && (refDecrypt || refSign    || refEncrypt))
	) {
		Trap0(RE_BAD_REFINES);
	}

	if (NOT_VALID_CONTEXT_HANDLE(key, SYM_RSA)) Trap0(RE_INVALID_HANDLE);

	rsa = (RSA_CTX*)VAL_HANDLE_CONTEXT_DATA(key);

	if (IS_NONE(val_data)) {
		// release RSA key resources
		Free_Hob(VAL_HANDLE_CTX(key));
		return R_TRUE;
	}

	if(
		(mbedtls_rsa_check_pubkey(rsa) != 0) ||
		((refDecrypt || refSign) && (mbedtls_rsa_check_privkey(rsa) != 0))
	) {
		return R_NONE;
	}

	data = VAL_SERIES(val_data);
	inBinary = BIN_DATA(data);
	inBytes = BIN_LEN(data);

	if (refVerify) {
		SHA256(inBinary, inBytes, hash);
		err = mbedtls_rsa_rsassa_pkcs1_v15_verify(rsa, MBEDTLS_MD_SHA256, 32, hash, VAL_BIN(val_sign));
		return (err == 0) ? R_TRUE : R_FALSE;
	}

	//allocate new binary!
	outBytes = mbedtls_rsa_get_len(rsa);
	data = Make_Binary(outBytes-1);
	outBinary = BIN_DATA(data);

	if (refSign) {
		SHA256(inBinary, inBytes, hash);
		err = mbedtls_rsa_rsassa_pkcs1_v15_sign(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_MD_SHA256, 32, hash, outBinary);
	}
	else if (refEncrypt) {
		err = mbedtls_rsa_rsaes_pkcs1_v15_encrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, inBytes, inBinary, outBinary);
	}
	else {
		size_t olen = 0;
		err = mbedtls_rsa_rsaes_pkcs1_v15_decrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, &olen, inBinary, outBinary, outBytes);
		outBytes = olen;
	}
	if (err) goto error;

	SET_BINARY(D_RET, data);
	VAL_TAIL(D_RET) = outBytes;

	return R_RET;
error:
	//printf("RSA key init failed with error:: -0x%0x\n", (unsigned int)-err);
	Free_Series(data);
	return R_NONE;
}


/***********************************************************************
**
*/	REBNATIVE(dh_init)
/*
//  dh-init: native [
//		"Generates a new Diffie-Hellman private/public key pair"
//		g [binary!] "Generator"
//		p [binary!] "Field prime"
//  ]
***********************************************************************/
{
	REBVAL *g        = D_ARG(1);
	REBVAL *p        = D_ARG(2);
	REBOOL  ref_into = D_REF(3);
	REBVAL *val_dh   = D_ARG(4);
	
	REBINT  err = 0;
	DHM_CTX *dhm;

	MAKE_HANDLE(D_RET, SYM_DHM);
	dhm = (DHM_CTX*)VAL_HANDLE_CONTEXT_DATA(D_RET);
	mbedtls_dhm_init(dhm);

	err = mbedtls_mpi_read_binary(&dhm->MBEDTLS_PRIVATE(P), VAL_BIN_AT(p), VAL_SERIES(p)->tail - VAL_INDEX(p) );
	if (err) goto error;
	err = mbedtls_mpi_read_binary( &dhm->MBEDTLS_PRIVATE(G), VAL_BIN_AT(g), VAL_SERIES(g)->tail - VAL_INDEX(g));
	if (err) goto error;

	size_t n = mbedtls_dhm_get_len(dhm);
	if (n < 64 || n > 512) goto error;

	/* Generate private key X as large as possible ( <= P - 2 ) */
	err = dhm_random_below(
		&dhm->MBEDTLS_PRIVATE(X),
		&dhm->MBEDTLS_PRIVATE(P),
		mbedtls_ctr_drbg_random, &ctr_drbg);
	if (err) goto error;

	/*
	 * Calculate public key (self) GX = G^X mod P
	 */
	err = mbedtls_mpi_exp_mod(
		&dhm->MBEDTLS_PRIVATE(GX),
		&dhm->MBEDTLS_PRIVATE(G),
		&dhm->MBEDTLS_PRIVATE(X),
		&dhm->MBEDTLS_PRIVATE(P),
		&dhm->MBEDTLS_PRIVATE(RP));
	if (err) goto error;

	err = dhm_check_range(
		&dhm->MBEDTLS_PRIVATE(GX),
		&dhm->MBEDTLS_PRIVATE(P));
	if (err) goto error;

	return R_RET;

error:
	//printf("DHM key init failed with error: -0x%0x\n", (unsigned int)-err);
	Free_Hob(VAL_HANDLE_CTX(D_RET));
	return R_NONE;
}

/***********************************************************************
**
*/	REBNATIVE(dh)
/*
//  dh: native [
//		"Diffie-Hellman key exchange"
//		dh-key [handle!] "DH key created using `dh-init` function"
//		/release "Releases internal DH key resources"
//		/public  "Returns public key as a binary"
//		/secret  "Computes secret result using peer's public key"
//			public-key [binary!] "Peer's public key"
//  ]
***********************************************************************/
{
	REBVAL *key        = D_ARG(1);
	REBOOL  refRelease = D_REF(2);
	REBOOL  refPublic  = D_REF(3);
	REBOOL  refSecret  = D_REF(4);
	REBVAL *gy         = D_ARG(5);

	REBSER  *out = NULL;
	REBINT   err;
	DHM_CTX *dhm;
	size_t   gx_len, gy_len, olen = 0;

	if (refPublic && refSecret) {
		// only one can be used
		Trap0(RE_BAD_REFINES);
	}

	if (NOT_VALID_CONTEXT_HANDLE(key, SYM_DHM))
		return R_NONE;	//or? Trap0(RE_INVALID_HANDLE);

	dhm = (DHM_CTX *)VAL_HANDLE_CONTEXT_DATA(key);

	if (refPublic) {
		gx_len = mbedtls_mpi_size(&dhm->MBEDTLS_PRIVATE(GX));
		if (gx_len <= 0) goto error;
		out = Make_Binary(gx_len-1);
		mbedtls_mpi_write_binary(&dhm->MBEDTLS_PRIVATE(GX), BIN_DATA(out), gx_len);
		BIN_LEN(out) = gx_len;
	}
	if (refSecret) {
		// import oposite's side public data...
		gy_len = VAL_SERIES(gy)->tail - VAL_INDEX(gy);
		err = mbedtls_mpi_read_binary(&dhm->MBEDTLS_PRIVATE(GY), VAL_BIN_AT(gy), gy_len);
		if (err) goto error;
		out = Make_Binary(gy_len-1);
		// and derive the shared secret of these 2 public parts
		err = mbedtls_dhm_calc_secret(dhm, BIN_DATA(out), gy_len, &olen, mbedtls_ctr_drbg_random, &ctr_drbg);
		if (err) goto error;
		BIN_LEN(out) = olen;
	}
	if (refRelease) {
		Free_Hob(VAL_HANDLE_CTX(key));
		if (!out) return R_TRUE;
	}
	SET_BINARY(D_RET, out);
	return R_RET;
error:
	if (out != NULL) FREE_SERIES(out);
	return R_NONE;
}


static uECC_Curve get_ecc_curve(REBCNT curve_type) {
	uECC_Curve curve = NULL;
	switch (curve_type) {
		case SYM_SECP256K1:
			curve = ECC_curves[4];
			if(curve == NULL) {
				curve = uECC_secp256k1();
				ECC_curves[4] = curve;
			}
			break;
		case SYM_SECP256R1:
			curve = ECC_curves[3];
			if(curve == NULL) {
				curve = uECC_secp256r1();
				ECC_curves[3] = curve;
			}
			break;
		case SYM_SECP224R1:
			curve = ECC_curves[2];
			if(curve == NULL) {
				curve = uECC_secp224r1();
				ECC_curves[2] = curve;
			}
			break;
		case SYM_SECP192R1:
			curve = ECC_curves[1];
			if(curve == NULL) {
				curve = uECC_secp192r1();
				ECC_curves[1] = curve;
			}
			break;		
		case SYM_SECP160R1:
			curve = ECC_curves[0];
			if(curve == NULL) {
				curve = uECC_secp160r1();
				ECC_curves[0] = curve;
			}
			break;
	}
	return curve;
}

/***********************************************************************
**
*/	REBNATIVE(ecdh)
/*
//  ecdh: native [
//		"Elliptic-curve Diffie-Hellman key exchange"
//		key [handle! none!] "Keypair to work with, may be NONE for /init refinement"
//		/init   "Initialize ECC keypair."
//			type [word!] "One of supported curves: [secp256k1 secp256r1 secp224r1 secp192r1 secp160r1]"
//		/curve  "Returns handles curve type"
//		/public "Returns public key as a binary"
//		/secret  "Computes secret result using peer's public key"
//			public-key [binary!] "Peer's public key"
//		/release "Releases internal ECDH key resources"
//  ]
***********************************************************************/
{
	REBVAL *val_handle  = D_ARG(1);
	REBOOL  ref_init    = D_REF(2);
	REBVAL *val_curve   = D_ARG(3);
	REBOOL  ref_type    = D_REF(4);
	REBOOL  ref_public  = D_REF(5);
	REBOOL  ref_secret  = D_REF(6);
	REBVAL *val_public  = D_ARG(7);
	REBOOL  ref_release = D_REF(8);

	REBSER *bin = NULL;
	uECC_Curve curve = NULL;
	ECC_CTX *ecc = NULL;

	if (ref_init) {
		MAKE_HANDLE(val_handle, SYM_ECDH);
		ecc = (ECC_CTX*)VAL_HANDLE_CONTEXT_DATA(val_handle);
		ecc->curve_type = VAL_WORD_CANON(val_curve);
		curve = get_ecc_curve(ecc->curve_type);
		if (!curve) return R_NONE;
		if(!uECC_make_key(ecc->public, ecc->private, curve)) {
			//puts("failed to init ECDH key");
			return R_NONE;
		}
		else return R_ARG1;
	} else {
		if (NOT_VALID_CONTEXT_HANDLE(val_handle, SYM_ECDH)) {
			// not throwing an error.. just returning NONE
			return R_NONE;
		}
		ecc = (ECC_CTX*)VAL_HANDLE_CONTEXT_DATA(val_handle);
		curve = get_ecc_curve(ecc->curve_type);
		if (!curve) return R_NONE;
	}

	if (ref_secret) {
		bin = Make_Binary(32);
		if (!uECC_shared_secret(VAL_DATA(val_public), ecc->private, BIN_DATA(bin), curve)) {
			return R_NONE;
        }
		if(ref_release) {
			Free_Hob(VAL_HANDLE_CTX(val_handle));
		}
		SET_BINARY(D_RET, bin);
		BIN_LEN(bin) = 32;
		return R_RET;
	}

	if (ref_public) {
		bin = Make_Binary(64);
		COPY_MEM(BIN_DATA(bin), ecc->public, 64);
		SET_BINARY(D_RET, bin);
		BIN_LEN(bin) = 64;
		return R_RET;
	}

	if(ref_release) {
		Free_Hob(VAL_HANDLE_CTX(val_handle));
		return R_ARG1;
	}

	if (ref_type) {
		Init_Word(val_curve, ecc->curve_type);
		return R_ARG3;
	}
	return R_ARG1;
}


/***********************************************************************
**
*/	REBNATIVE(ecdsa)
/*
//  ecdsa: native [
//		"Elliptic Curve Digital Signature Algorithm"
//		key [handle! binary!] "Keypair to work with, created using ECDH function, or raw binary key (needs /curve)"
//		hash [binary!] "Data to sign or verify"
//		/sign   "Use private key to sign data, returns 64 bytes of signature"
//		/verify "Use public key to verify signed data, returns true or false"
//			signature [binary!] "Signature (64 bytes)"
//		/curve "Used if key is just a binary"
//			type [word!] "One of supported curves: [secp256k1 secp256r1 secp224r1 secp192r1 secp160r1]"
//  ]
***********************************************************************/
{
	REBVAL *val_key     = D_ARG(1);
	REBVAL *val_hash    = D_ARG(2);
	REBOOL  ref_sign    = D_REF(3);
	REBOOL  ref_verify  = D_REF(4);
	REBVAL *val_sign    = D_ARG(5);
	REBOOL  ref_curve   = D_REF(6);
	REBVAL *val_curve   = D_ARG(7);

	REBSER *bin = NULL;
	REBYTE *key = NULL;
	REBCNT curve_type = 0;
	uECC_Curve curve = NULL;
	ECC_CTX *ecc = NULL;

	if (IS_BINARY(val_key)) {
		if (!ref_curve) Trap0(RE_MISSING_ARG);
		curve_type = VAL_WORD_CANON(val_curve);
	}
	else {
		if (NOT_VALID_CONTEXT_HANDLE(val_key, SYM_ECDH)) {
			Trap0(RE_INVALID_HANDLE);
		}
		ecc = (ECC_CTX*)VAL_HANDLE_CONTEXT_DATA(val_key);
		curve_type = ecc->curve_type;
	}

	curve = get_ecc_curve(curve_type);
	if (!curve) return R_NONE;

	if (ref_sign) {
		if (ecc) {
			key = ecc->private;
		}
		else {
			if (VAL_LEN(val_key) != 32) return R_NONE;
			key = VAL_BIN(val_key);
		}
		bin = Make_Series(64, 1, FALSE);
		if(!uECC_sign(key, VAL_DATA(val_hash), VAL_LEN(val_hash), BIN_DATA(bin), curve)) {
			return R_NONE;
		}
		SET_BINARY(D_RET, bin);
		VAL_TAIL(D_RET) = 64;
		return R_RET;
	}

	if (ref_verify) {
		if (ecc) {
			key = ecc->public;
		}
		else {
			if (VAL_LEN(val_key) != 64) return R_FALSE;
			key = VAL_BIN(val_key);
		}
		if(VAL_LEN(val_sign) == 64 && uECC_verify(key, VAL_DATA(val_hash), VAL_LEN(val_hash), VAL_DATA(val_sign), curve)) {
			return R_TRUE;
		}
		else {
			return R_FALSE;
		}
	}
	return R_UNSET;
}


/***********************************************************************
**
*/	REBNATIVE(chacha20)
/*
//  chacha20: native [
//		"Encrypt/decrypt data using ChaCha20 algorithm. Returns stream cipher context handle or encrypted/decrypted data."
//		ctx [handle! binary!] "ChaCha20 handle and or binary key for initialization (16 or 32 bytes)"
//		/init
//			nonce [binary!] "Initialization nonce (IV) - 8 or 12 bytes."
//			count [integer!] "A 32-bit block count parameter"
//		/aad sequence [integer!] "Sequence number used with /init to modify nonce"
//		/stream
//			data [binary!]  "Data to encrypt/decrypt."
//		/into
//			out [binary!]   "Output buffer (NOT YET IMPLEMENTED)"
//  ]
***********************************************************************/
{
#ifdef EXCLUDE_CHACHA20POLY1305
	Trap0(RE_FEATURE_NA);
#else
	REBVAL *val_ctx       = D_ARG(1);
	REBOOL  ref_init      = D_REF(2);
    REBVAL *val_nonce     = D_ARG(3);
	REBVAL *val_counter   = D_ARG(4);
	REBOOL  ref_aad       = D_REF(5);
	REBVAL *val_sequence  = D_ARG(6);
    REBOOL  ref_stream    = D_REF(7);
    REBVAL *val_data      = D_ARG(8);
	REBOOL  ref_into      = D_REF(9);
	
	if(ref_into)
		Trap0(RE_FEATURE_NA);

	REBINT  len;
	REBU64  sequence;

	if (IS_BINARY(val_ctx)) {
		len = VAL_LEN(val_ctx);
		if (!(len == 32 || len == 16)) {
			//printf("ChaCha20 key must be of size 32 or 16 bytes! Is: %i\n", len);
			Trap1(RE_INVALID_DATA, val_ctx);
			return R_NONE;
		}
		REBYTE *bin_key = VAL_BIN_AT(val_ctx);
		
		MAKE_HANDLE(val_ctx, SYM_CHACHA20);
		chacha20_keysetup((chacha20_ctx*)VAL_HANDLE_CONTEXT_DATA(val_ctx), bin_key, len);
	}
	else {
		if (NOT_VALID_CONTEXT_HANDLE(val_ctx, SYM_CHACHA20)) {
    		Trap0(RE_INVALID_HANDLE);
			return R_NONE; // avoid wornings later
		}
	}

	if (ref_init) {
		// initialize nonce with counter
		
		len = VAL_LEN(val_nonce);

		if (!(len == 12 || len == 8)) {
			Trap1(RE_INVALID_DATA, val_nonce);
			return R_NONE;
		}

		sequence = (ref_aad) ? VAL_INT64(val_sequence) : 0;
		chacha20_ivsetup((chacha20_ctx*)VAL_HANDLE_CONTEXT_DATA(val_ctx), VAL_BIN_AT(val_nonce), len, VAL_INT64(val_counter), (u8 *)&sequence);
    }
	
	if (ref_stream) {

    	len = VAL_LEN(val_data);
    	if (len == 0) return R_NONE;

		REBYTE *data = VAL_BIN_AT(val_data);
		REBSER  *binaryOut = Make_Binary(len);

		chacha20_encrypt(
			(chacha20_ctx *)VAL_HANDLE_CONTEXT_DATA(val_ctx),
			(const uint8_t*)data,
			(      uint8_t*)BIN_DATA(binaryOut),
			len
		);

		SET_BINARY(val_ctx, binaryOut);
		VAL_TAIL(val_ctx) = len;

    }
#endif // EXCLUDE_CHACHA20POLY1305
	return R_ARG1;
}



/***********************************************************************
**
*/	REBNATIVE(poly1305)
/*
//  poly1305: native [
//		"poly1305 message-authentication"
//		ctx [handle! binary!] "poly1305 handle and or binary key for initialization (32 bytes)"
//		/update data [binary!] "data to authenticate"
//		/finish                "finish data stream and return raw result as a binary"
//		/verify                "finish data stream and compare result with expected result (MAC)"
//			mac      [binary!] "16 bytes of verification MAC"
//  ]
***********************************************************************/
{
#ifdef EXCLUDE_CHACHA20POLY1305
	Trap0(RE_FEATURE_NA);
#else
	REBVAL *val_ctx       = D_ARG(1);
	REBOOL  ref_update    = D_REF(2);
	REBVAL *val_data      = D_ARG(3);
	REBOOL  ref_finish    = D_REF(4);
	REBOOL  ref_verify    = D_REF(5);
	REBVAL *val_mac       = D_ARG(6);
    
    REBVAL *ret = D_RET;
//	REBSER *ctx_ser;
	REBINT  len;
//	REBCNT  i;
	REBYTE  mac[16];

	if (IS_BINARY(val_ctx)) {
		len = VAL_LEN(val_ctx);
		if (len < POLY1305_KEYLEN) {
			Trap1(RE_INVALID_DATA, val_ctx);
			return R_NONE;
		}
		
		REBYTE *bin_key = VAL_BIN_AT(val_ctx);
		
		MAKE_HANDLE(val_ctx, SYM_POLY1305);
		poly1305_init((poly1305_context*)VAL_HANDLE_CONTEXT_DATA(val_ctx), bin_key);
	}
	else {
		if (NOT_VALID_CONTEXT_HANDLE(val_ctx, SYM_POLY1305)) {
    		Trap0(RE_INVALID_HANDLE);
		}
	}

	if (ref_update) {
		poly1305_update((poly1305_context*)VAL_HANDLE_CONTEXT_DATA(val_ctx), VAL_BIN_AT(val_data), VAL_LEN(val_data));
	}

	if (ref_finish) {
		SET_BINARY(ret, Make_Series(16, 1, FALSE));
		VAL_TAIL(ret) = 16;
		poly1305_finish((poly1305_context*)VAL_HANDLE_CONTEXT_DATA(val_ctx), VAL_BIN(ret));
		return R_RET;
	}

	if (ref_verify) {
		if (VAL_LEN(val_mac) != POLY1305_TAGLEN)
			return R_FALSE; // or error?
		CLEARS(mac);
		poly1305_finish((poly1305_context*)VAL_HANDLE_CONTEXT_DATA(val_ctx), mac);
		return (poly1305_verify(VAL_BIN_AT(val_mac), mac)) ? R_TRUE : R_FALSE;
	}

#endif // EXCLUDE_CHACHA20POLY1305
	return R_ARG1;
}

/***********************************************************************
**
*/	REBNATIVE(chacha20poly1305)
/*
//  chacha20poly1305: native [
//		"..."
//		ctx [none! handle!]
//		/init 
//			local-key     [binary!]
//			local-iv      [binary!]
//			remote-key    [binary!]
//			remote-iv     [binary!]
//		/encrypt
//			data-out      [binary!]
//			aad-out       [binary!]
//		/decrypt
//			data-in       [binary!]
//			aad-in        [binary!]
//  ]
***********************************************************************/
{
#ifdef EXCLUDE_CHACHA20POLY1305
	Trap0(RE_FEATURE_NA);
#else
	REBVAL *val_ctx        = D_ARG(1);
	REBOOL  ref_init       = D_REF(2);
	REBVAL *val_local_key  = D_ARG(3);
	REBVAL *val_local_iv   = D_ARG(4);
	REBVAL *val_remote_key = D_ARG(5);
	REBVAL *val_remote_iv  = D_ARG(6);
	REBOOL  ref_encrypt    = D_REF(7);
	REBVAL *val_plain      = D_ARG(8);
	REBVAL *val_local_aad  = D_ARG(9);
	REBOOL  ref_decrypt    = D_REF(10);
	REBVAL *val_cipher     = D_ARG(11);
	REBVAL *val_remote_aad = D_ARG(12);

	REBSER *ctx_ser;
	REBINT  len;
	chacha20poly1305_ctx *chacha;
	unsigned char poly1305_key[POLY1305_KEYLEN];
	size_t aad_size;
	REBU64 sequence = 0;

	if (ref_init) {
		MAKE_HANDLE(val_ctx, SYM_CHACHA20POLY1305);

		chacha = (chacha20poly1305_ctx*)VAL_HANDLE_CONTEXT_DATA(val_ctx);
		len = VAL_LEN(val_local_key);
		if (!(len == 32 || len == 16))
			Trap1(RE_INVALID_DATA, val_local_key);
		chacha20_keysetup(&chacha->local_chacha, VAL_BIN_AT(val_local_key), len);
		len = VAL_LEN(val_remote_key);
		if (!(len == 32 || len == 16))
			Trap1(RE_INVALID_DATA, val_remote_key);
		chacha20_keysetup(&chacha->remote_chacha, VAL_BIN_AT(val_remote_key), len);

		len = VAL_LEN(val_local_iv);
		if (!(len == 12 || len == 8))
			Trap1(RE_INVALID_DATA, val_local_iv);
		chacha20_ivsetup(&chacha->local_chacha, VAL_BIN_AT(val_local_iv), len, 1, (u8 *)&sequence);
		memcpy(chacha->local_iv, VAL_BIN_AT(val_local_iv), len);

		len = VAL_LEN(val_remote_iv);
		if (!(len == 12 || len == 8))
			Trap1(RE_INVALID_DATA, val_remote_iv);
		chacha20_ivsetup(&chacha->remote_chacha, VAL_BIN_AT(val_remote_iv), len, 1, (u8 *)&sequence);
		memcpy(chacha->remote_iv, VAL_BIN_AT(val_remote_iv), len);
		return R_ARG1;
	}

	if (NOT_VALID_CONTEXT_HANDLE(val_ctx, SYM_CHACHA20POLY1305)) {
    	Trap0(RE_INVALID_HANDLE);
		return R_NONE;
	}
	chacha = (chacha20poly1305_ctx*)VAL_HANDLE_CONTEXT_DATA(val_ctx);

	if (ref_encrypt) {
		chacha20_ivsetup(&chacha->local_chacha, chacha->local_iv, 12, 1,  VAL_BIN_AT(val_local_aad));
		chacha20_poly1305_key(&chacha->local_chacha, poly1305_key);
		//puts("poly1305_key:"); Dump_Bytes(poly1305_key, POLY1305_KEYLEN);
		
		len = VAL_LEN(val_plain) + POLY1305_TAGLEN;
		ctx_ser = Make_Series(len, (REBCNT)1, FALSE);

		// AEAD
		// sequence number (8 bytes)
		// content type (1 byte)
		// version (2 bytes)
		// length (2 bytes)
		unsigned char aad[13];
		aad_size = sizeof(aad);
//		unsigned char *sequence = aad;

		chacha20_poly1305_aead(&chacha->local_chacha, VAL_BIN_AT(val_plain), (REBCNT)len-POLY1305_TAGLEN, VAL_BIN_AT(val_local_aad), VAL_LEN(val_local_aad), poly1305_key, ctx_ser->data);

		//chacha->local_sequence++;

		SERIES_TAIL(ctx_ser) = len;
		SET_BINARY(val_ctx, ctx_ser);
		return R_ARG1;
	}

	if (ref_decrypt) {
		static unsigned char zeropad[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		unsigned char trail[16];
		unsigned char mac_tag[POLY1305_TAGLEN];

		chacha20_ivsetup(&chacha->remote_chacha, chacha->remote_iv, 12, 1, VAL_BIN_AT(val_remote_aad));

		len = VAL_LEN(val_cipher) - POLY1305_TAGLEN;
		if (len <= 0)
			return R_NONE;

		ctx_ser = Make_Series(len, (REBCNT)1, FALSE);

		//puts("\nDECRYPT:");

		chacha20_encrypt(&chacha->remote_chacha, VAL_BIN_AT(val_cipher), ctx_ser->data, len);
		//chacha_encrypt_bytes(&chacha->remote_chacha, VAL_BIN_AT(val_cipher), ctx_ser->data, len);
		chacha20_poly1305_key(&chacha->remote_chacha, poly1305_key);
		//puts("poly1305_key:"); Dump_Bytes(poly1305_key, POLY1305_KEYLEN);

		poly1305_context aead_ctx;
		poly1305_init(&aead_ctx, poly1305_key);

		aad_size = VAL_LEN(val_remote_aad);
		poly1305_update(&aead_ctx, VAL_BIN_AT(val_remote_aad), aad_size);
		int rem = aad_size % 16;
        if (rem)
            poly1305_update(&aead_ctx, zeropad, 16 - rem);
		//puts("update:"); Dump_Bytes(VAL_BIN_AT(val_cipher), len);
        poly1305_update(&aead_ctx, VAL_BIN_AT(val_cipher), len);
        rem = len % 16;
        if (rem)
            poly1305_update(&aead_ctx, zeropad, 16 - rem);
            
        U32TO8_LE(&trail[0], aad_size == 5 ? 5 : 13);
        *(int *)&trail[4] = 0;
        U32TO8_LE(&trail[8], len);
        *(int *)&trail[12] = 0;

		//puts("trail:"); Dump_Bytes(trail, 16);

        poly1305_update(&aead_ctx, trail, 16);
        poly1305_finish(&aead_ctx, mac_tag);

		if (!poly1305_verify(mac_tag, VAL_BIN_TAIL(val_cipher) - POLY1305_TAGLEN)) {
			//puts("MAC verification failed!");
			return R_NONE;
		}

		//puts("mac result:"); Dump_Bytes(mac_tag, POLY1305_TAGLEN);

		SERIES_TAIL(ctx_ser) = len;
		SET_BINARY(val_ctx, ctx_ser);
	}
#endif // EXCLUDE_CHACHA20POLY1305
	return R_ARG1;
}
