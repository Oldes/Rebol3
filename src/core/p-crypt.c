/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  Copyright 2012-2022 Rebol Open Source Contributors
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
**  Module:  p-crypt.c
**  Summary: Cryptography port interface
**  Section: ports
**  Author:  Oldes
**  Notes:
**
***********************************************************************/

#if !defined(REBOL_OPTIONS_FILE)
#include "opt-config.h"
#else
#include REBOL_OPTIONS_FILE
#endif

#include "sys-crypt.h"
#include "reb-net.h"

#ifdef INCLUDE_MBEDTLS

#ifndef MBEDTLS_GET_UINT32_BE
// including it here as it is defined only for private use in the mbedTLS sources
#define MBEDTLS_GET_UINT32_BE( data , offset )                  \
    (                                                           \
          ( (uint32_t) ( data )[( offset )    ] << 24 )         \
        | ( (uint32_t) ( data )[( offset ) + 1] << 16 )         \
        | ( (uint32_t) ( data )[( offset ) + 2] <<  8 )         \
        | ( (uint32_t) ( data )[( offset ) + 3]       )         \
    )
#endif

static void free_crypt_cipher_context(CRYPT_CTX *ctx);


/***********************************************************************
**
*/	void crypt_context_free(void *ctx)
/*
***********************************************************************/
{
	CRYPT_CTX *crypt;
	if (ctx == NULL) return;
	crypt = (CRYPT_CTX *)ctx;
	if (crypt->buffer) {
		CLEAR(crypt->buffer->data, crypt->buffer->rest);
		Free_Series(crypt->buffer);
	}
	free_crypt_cipher_context(crypt);
	CLEARS(crypt);
}


/***********************************************************************
**
*/	static REBOOL init_crypt_key(CRYPT_CTX *ctx, REBVAL *val)
/*
***********************************************************************/
{
	REBSER *ser;
	REBCNT  len = 0;
	REBYTE *bin = NULL;
	if (val == NULL) return FALSE;
	ctx->state = CRYPT_PORT_NEEDS_INIT;
	if (IS_NONE(val)) {
		CLEAR(&ctx->key, MBEDTLS_MAX_KEY_LENGTH);
		return TRUE;
	}
	if (IS_STRING(val)) {
		ser = Encode_UTF8_Value(val, VAL_LEN(val), 0);
		len = SERIES_TAIL(ser);
		bin = BIN_HEAD(ser);
	}
	else if (IS_BINARY(val)) {
		len = VAL_LEN(val);
		bin = VAL_BIN_AT(val);
	}
	if (bin && len > 0) {
		if (len > MBEDTLS_MAX_KEY_LENGTH)
			len = MBEDTLS_MAX_KEY_LENGTH;
		CLEAR(&ctx->key, MBEDTLS_MAX_KEY_LENGTH);
		COPY_MEM(&ctx->key, bin, len);
		return TRUE;
	}
	return FALSE;
}

/***********************************************************************
**
*/	static REBOOL init_crypt_iv(CRYPT_CTX *ctx, REBVAL *val)
/*
***********************************************************************/
{
	REBCNT len;
	if (val == NULL) return FALSE;
	ctx->state = CRYPT_PORT_NEEDS_INIT;
	if (IS_NONE(val)) {
		CLEAR(&ctx->IV, MBEDTLS_MAX_IV_LENGTH);
		CLEAR(&ctx->nonce, MBEDTLS_MAX_IV_LENGTH);
		return TRUE;
	}
	if (IS_BINARY(val)) {
		len = VAL_LEN(val);
		if (len > 0) {
			if (len > MBEDTLS_MAX_IV_LENGTH)
				len = MBEDTLS_MAX_IV_LENGTH;
			CLEAR(&ctx->IV, MBEDTLS_MAX_IV_LENGTH);
			CLEAR(&ctx->nonce, MBEDTLS_MAX_IV_LENGTH);
			COPY_MEM(&ctx->IV, VAL_BIN_AT(val), len);
		}
		return TRUE;
	}
	return FALSE;
}

/***********************************************************************
**
*/	static REBOOL init_crypt_direction(CRYPT_CTX *ctx, REBVAL *val)
/*
***********************************************************************/
{
	if (!val || !IS_WORD(val)) return FALSE;
	ctx->state = CRYPT_PORT_NEEDS_INIT;
	switch (VAL_WORD_CANON(val)) {
	case SYM_ENCRYPT:
		ctx->operation = MBEDTLS_ENCRYPT;
		break;
	case SYM_DECRYPT:
		ctx->operation = MBEDTLS_DECRYPT;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

/***********************************************************************
**
*/	static void free_crypt_cipher_context(CRYPT_CTX *ctx)
/*
***********************************************************************/
{
	REBCNT type = ctx->cipher_type;
	if (ctx->cipher_ctx == NULL) return;
	ctx->state = CRYPT_PORT_CLOSED;
	switch (type) {
	case SYM_AES_128_ECB:
	case SYM_AES_192_ECB:
	case SYM_AES_256_ECB:
	case SYM_AES_128_CBC:
	case SYM_AES_192_CBC:
	case SYM_AES_256_CBC:
		mbedtls_aes_free((mbedtls_aes_context *)ctx->cipher_ctx);
		break;


#ifdef MBEDTLS_CAMELLIA_C
	case SYM_CAMELLIA_128_ECB:
	case SYM_CAMELLIA_192_ECB:
	case SYM_CAMELLIA_256_ECB:
	case SYM_CAMELLIA_128_CBC:
	case SYM_CAMELLIA_192_CBC:
	case SYM_CAMELLIA_256_CBC:
		mbedtls_camellia_free((mbedtls_camellia_context *)ctx->cipher_ctx);
		break;
#endif


#ifdef MBEDTLS_ARIA_C
	case SYM_ARIA_128_ECB:
	case SYM_ARIA_192_ECB:
	case SYM_ARIA_256_ECB:
	case SYM_ARIA_128_CBC:
	case SYM_ARIA_192_CBC:
	case SYM_ARIA_256_CBC:
		mbedtls_aria_free((mbedtls_aria_context *)ctx->cipher_ctx);
		break;
#endif


#ifdef MBEDTLS_CHACHA20_C
	case SYM_CHACHA20:
		mbedtls_chacha20_free((mbedtls_chacha20_context *)ctx->cipher_ctx);
		break;
#endif


#ifdef MBEDTLS_CHACHAPOLY_C
	case SYM_CHACHA20_POLY1305:
		mbedtls_chachapoly_free((mbedtls_chachapoly_context *)ctx->cipher_ctx);
		break;
#endif
	}
	free(ctx->cipher_ctx);
	ctx->cipher_ctx = NULL;
}



/***********************************************************************
**
*/	static REBOOL init_crypt_algorithm(CRYPT_CTX *ctx, REBVAL *val)
/*
***********************************************************************/
{
	REBCNT type;

	if (!IS_WORD(val)) return FALSE;
	type = VAL_WORD_CANON(val);
	if (type != ctx->cipher_type) {
		free_crypt_cipher_context(ctx);
	}
	ctx->state = CRYPT_PORT_NEEDS_INIT;
	switch (type) {

	case SYM_AES_128_ECB:
	case SYM_AES_192_ECB:
	case SYM_AES_256_ECB:
#ifdef MBEDTLS_CIPHER_MODE_CBC
	case SYM_AES_128_CBC:
	case SYM_AES_192_CBC:
	case SYM_AES_256_CBC:
#endif
		if (ctx->cipher_ctx == NULL)
			ctx->cipher_ctx = malloc(sizeof(mbedtls_aes_context));
		mbedtls_aes_init((mbedtls_aes_context*)ctx->cipher_ctx);
		switch (type) {
		case SYM_AES_128_ECB:
		case SYM_AES_128_CBC: ctx->key_bitlen = 128; break;
		case SYM_AES_192_ECB:
		case SYM_AES_192_CBC: ctx->key_bitlen = 192; break;
		case SYM_AES_256_ECB:
		case SYM_AES_256_CBC: ctx->key_bitlen = 256; break;
		}
		ctx->cipher_block_size = 16;
		break;


#ifdef MBEDTLS_CAMELLIA_C
	case SYM_CAMELLIA_128_ECB:
	case SYM_CAMELLIA_192_ECB:
	case SYM_CAMELLIA_256_ECB:
#ifdef MBEDTLS_CIPHER_MODE_CBC
	case SYM_CAMELLIA_128_CBC:
	case SYM_CAMELLIA_192_CBC:
	case SYM_CAMELLIA_256_CBC:
#endif
		if (ctx->cipher_ctx == NULL)
			ctx->cipher_ctx = malloc(sizeof(mbedtls_camellia_context));
		switch (type) {
		case SYM_CAMELLIA_128_ECB:
		case SYM_CAMELLIA_128_CBC: ctx->key_bitlen = 128; break;
		case SYM_CAMELLIA_192_ECB:
		case SYM_CAMELLIA_192_CBC: ctx->key_bitlen = 192; break;
		case SYM_CAMELLIA_256_ECB:
		case SYM_CAMELLIA_256_CBC: ctx->key_bitlen = 256; break;
		}
		ctx->cipher_block_size = 16;
		break;
#endif


#ifdef MBEDTLS_ARIA_C
	case SYM_ARIA_128_ECB:
	case SYM_ARIA_192_ECB:
	case SYM_ARIA_256_ECB:
#ifdef MBEDTLS_CIPHER_MODE_CBC
	case SYM_ARIA_128_CBC:
	case SYM_ARIA_192_CBC:
	case SYM_ARIA_256_CBC:
#endif
		if (ctx->cipher_ctx == NULL)
			ctx->cipher_ctx = malloc(sizeof(mbedtls_camellia_context));
		switch (type) {
		case SYM_ARIA_128_ECB:
		case SYM_ARIA_128_CBC: ctx->key_bitlen = 128; break;
		case SYM_ARIA_192_ECB:
		case SYM_ARIA_192_CBC: ctx->key_bitlen = 192; break;
		case SYM_ARIA_256_ECB:
		case SYM_ARIA_256_CBC: ctx->key_bitlen = 256; break;
		}
		ctx->cipher_block_size = 16;
		break;
#endif


#ifdef MBEDTLS_CHACHA20_C
	case SYM_CHACHA20:
		if (ctx->cipher_ctx == NULL)
			ctx->cipher_ctx = malloc(sizeof(mbedtls_chacha20_context));
		mbedtls_chacha20_init((mbedtls_chacha20_context *)ctx->cipher_ctx);
		ctx->cipher_block_size = 16U;
		break;
#endif


#ifdef MBEDTLS_CHACHAPOLY_C
	case SYM_CHACHA20_POLY1305:
		if (ctx->cipher_ctx == NULL)
			ctx->cipher_ctx = malloc(sizeof(CHACHAPOLY_CTX));
		mbedtls_chachapoly_init((CHACHAPOLY_CTX *)ctx->cipher_ctx);
		ctx->cipher_block_size = 0;
		break;
#endif
	}
	ctx->cipher_type = type;
	return TRUE;
}


/***********************************************************************
**
*/	static REBOOL Crypt_Open(REBSER *port)
/*
***********************************************************************/
{
	REBVAL *spec;
	REBVAL *state;
	REBVAL *val;
	REBVAL *val_key;
	REBYTE *name;
	REBINT  err = 0;
	REBCNT  len;
	REBINT  i;
	CRYPT_CTX *ctx;

	spec = BLK_SKIP(port, STD_PORT_SPEC);
	if (!IS_OBJECT(spec)) Trap1(RE_INVALID_SPEC, spec);

	state = BLK_SKIP(port, STD_PORT_STATE);
	MAKE_HANDLE(state, SYM_CRYPT);
	ctx = (CRYPT_CTX *)VAL_HANDLE_CONTEXT_DATA(state);
	
	if (NOT_VALID_CONTEXT_HANDLE(state, SYM_CRYPT)) {
		Trap0(RE_INVALID_HANDLE);
		return FALSE;
	}

	val = Obj_Value(spec, STD_PORT_SPEC_CRYPT_ALGORITHM);
	if (!init_crypt_algorithm(ctx, val)) { err = 1;  goto failed; }

	val = Obj_Value(spec, STD_PORT_SPEC_CRYPT_INIT_VECTOR);
	if (!init_crypt_iv(ctx, val)) { err = 1; goto failed; }
	SET_NONE(val); // as we have a copy, make it invisible from the spec

	val = Obj_Value(spec, STD_PORT_SPEC_CRYPT_KEY);
	if(!init_crypt_key(ctx, val)) { err = 1; goto failed; }
	SET_NONE(val); // as we have a copy, make it invisible from the spec

	val = Obj_Value(spec, STD_PORT_SPEC_CRYPT_DIRECTION);
	if (!init_crypt_direction(ctx, val)) { err = 1; goto failed; }

	ctx->buffer = Make_Binary(256);
	// buffer is extended when needed.
	// protected using KEEP, because it is not accesible from any real Rebol value!
	KEEP_SERIES(ctx->buffer, "crypt");

	ctx->state = CRYPT_PORT_NEEDS_INIT;

	return TRUE;

failed:
	if (IS_HANDLE(spec))
		Free_Hob(VAL_HANDLE_CTX(spec));
	if (err == 1) {
		Trap1(RE_INVALID_SPEC, spec);
	}
	Trap_Port(RE_CANNOT_OPEN, port, err);
	return FALSE;
}


/***********************************************************************
**
*/	static REBINT Crypt_Crypt(CRYPT_CTX *ctx, REBYTE *input, REBCNT len, REBCNT *olen)
/*
***********************************************************************/
{
	REBINT  err;
	REBSER *bin;
	REBCNT  blk, ofs;
	REBYTE *start = input;

	*olen = 0;

	if (len == 0) return 0;

	bin = ctx->buffer;
	blk = ctx->cipher_block_size;

	// make sure, that input is not less than required block size
	// unhandled input data are stored later in the ctx->unprocessed_data
	if (blk > 0 && len < blk) return 0;

	// make space at tail if needed...
	ofs = SERIES_TAIL(bin);
	Expand_Series(bin, AT_TAIL, len);
	// reset the tail (above expand modifies it!)
	SERIES_TAIL(bin) = ofs;

	switch (ctx->cipher_type) {

	case SYM_AES_128_ECB:
	case SYM_AES_192_ECB:
	case SYM_AES_256_ECB:
		for (ofs = 0; ofs <= len - blk; ofs += blk) {
			err = mbedtls_aes_crypt_ecb((mbedtls_aes_context *)ctx->cipher_ctx, ctx->operation, input, BIN_TAIL(bin));
			if (err) return err;
			SERIES_TAIL(bin) += blk;
			input += blk;
		}
		break;
#ifdef MBEDTLS_CIPHER_MODE_CBC
	case SYM_AES_128_CBC:
	case SYM_AES_192_CBC:
	case SYM_AES_256_CBC:
		blk = len - (len % 16);
		err = mbedtls_aes_crypt_cbc((mbedtls_aes_context *)ctx->cipher_ctx, ctx->operation, blk, ctx->IV, input, BIN_TAIL(bin));
		if (err) return err;
		SERIES_TAIL(bin) += blk;
		input += blk;
		break;
#endif

#ifdef MBEDTLS_CAMELLIA_C
	case SYM_CAMELLIA_128_ECB:
	case SYM_CAMELLIA_192_ECB:
	case SYM_CAMELLIA_256_ECB:
		for (ofs = 0; ofs <= len - blk; ofs += blk) {
			err = mbedtls_camellia_crypt_ecb((mbedtls_camellia_context *)ctx->cipher_ctx, ctx->operation, input, BIN_TAIL(bin));
			if (err) return err;
			SERIES_TAIL(bin) += blk;
			input += blk;
		}
		break;
#ifdef MBEDTLS_CIPHER_MODE_CBC
	case SYM_CAMELLIA_128_CBC:
	case SYM_CAMELLIA_192_CBC:
	case SYM_CAMELLIA_256_CBC:
		blk = len - (len % blk);
		err = mbedtls_camellia_crypt_cbc((mbedtls_camellia_context *)ctx->cipher_ctx, ctx->operation, blk, ctx->nonce, input, BIN_TAIL(bin));
		if (err) return err;
		SERIES_TAIL(bin) += blk;
		input += blk;
		break;
#endif
#endif


#ifdef MBEDTLS_ARIA_C
	case SYM_ARIA_128_ECB:
	case SYM_ARIA_192_ECB:
	case SYM_ARIA_256_ECB:
		for (ofs = 0; ofs <= len - blk; ofs += blk) {
			err = mbedtls_aria_crypt_ecb((mbedtls_aria_context *)ctx->cipher_ctx, input, BIN_TAIL(bin));
			if (err) return err;
			SERIES_TAIL(bin) += blk;
			input += blk;
		}
		break;
#ifdef MBEDTLS_CIPHER_MODE_CBC
	case SYM_ARIA_128_CBC:
	case SYM_ARIA_192_CBC:
	case SYM_ARIA_256_CBC:
		blk = len - (len % blk);
		err = mbedtls_aria_crypt_cbc((mbedtls_aria_context *)ctx->cipher_ctx, ctx->operation, blk, ctx->nonce, input, BIN_TAIL(bin));
		if (err) return err;
		SERIES_TAIL(bin) += blk;
		input += blk;
		break;
#endif
#endif


#ifdef MBEDTLS_CHACHA20_C
	case SYM_CHACHA20:
		err = mbedtls_chacha20_update((mbedtls_chacha20_context *)ctx->cipher_ctx, len, input, BIN_TAIL(bin));
		if (err) return err;
		SERIES_TAIL(bin) += len;
		input += len;
		break;
#endif

#ifdef MBEDTLS_CHACHAPOLY_C
	case SYM_CHACHA20_POLY1305:
		if (ctx->state == CRYPT_PORT_NEEDS_AAD) {
			size_t i;
			size_t dynamic_iv_len = len < 8 ? len : 8;
			unsigned char *dst_iv;
			dst_iv = ctx->nonce;
			memset(dst_iv, 0, 12);
			memcpy(dst_iv, ctx->IV, 12);
			dst_iv += 12 - dynamic_iv_len;
			for (i = 0; i < dynamic_iv_len; i++)
				dst_iv[i] ^= input[i];

			// https://github.com/ARMmbed/mbedtls/issues/5474
			mbedtls_chachapoly_mode_t mode = ctx->operation == MBEDTLS_ENCRYPT ? MBEDTLS_CHACHAPOLY_ENCRYPT : MBEDTLS_CHACHAPOLY_DECRYPT;
			
			err = mbedtls_chachapoly_starts((CHACHAPOLY_CTX *)ctx->cipher_ctx, ctx->nonce, mode);
			if (err) return err;

			err = mbedtls_chachapoly_update_aad((mbedtls_chachapoly_context *)ctx->cipher_ctx, input, len);
			if (err) return err;
			*olen = len;
			ctx->state = CRYPT_PORT_READY;
			// exit, because aad is not part of the output
			return CRYPT_OK;
		}
		else {
			err = mbedtls_chachapoly_update((CHACHAPOLY_CTX *)ctx->cipher_ctx, len, input, BIN_TAIL(bin));
		}
		if (err) return err;
		SERIES_TAIL(bin) += len;
		input += len;
		break;
#endif
	}

	*olen = input - start;
	return CRYPT_OK;
}

/***********************************************************************
**
*/	static REBINT Crypt_Init(CRYPT_CTX *ctx)
/*
***********************************************************************/
{
	REBINT  err = 0;
	REBCNT  counter;

	CLEAR_SERIES(ctx->buffer);
	SERIES_TAIL(ctx->buffer) = 0;
	CLEAR(ctx->unprocessed_data, MBEDTLS_MAX_BLOCK_LENGTH);
	ctx->unprocessed_len = 0;

	switch (ctx->cipher_type) {

	case SYM_AES_128_ECB:
	case SYM_AES_192_ECB:
	case SYM_AES_256_ECB:
	case SYM_AES_128_CBC:
	case SYM_AES_192_CBC:
	case SYM_AES_256_CBC:
		if (ctx->operation == MBEDTLS_ENCRYPT) {
			err = mbedtls_aes_setkey_enc((mbedtls_aes_context *)ctx->cipher_ctx, ctx->key, ctx->key_bitlen);
		}
		else {
			err = mbedtls_aes_setkey_dec((mbedtls_aes_context *)ctx->cipher_ctx, ctx->key, ctx->key_bitlen);
		}
		break;


	#ifdef MBEDTLS_CAMELLIA_C
	case SYM_CAMELLIA_128_ECB:
	case SYM_CAMELLIA_192_ECB:
	case SYM_CAMELLIA_256_ECB:
	case SYM_CAMELLIA_128_CBC:
	case SYM_CAMELLIA_192_CBC:
	case SYM_CAMELLIA_256_CBC:
		mbedtls_camellia_init((mbedtls_camellia_context *)ctx->cipher_ctx);
		if (ctx->operation == MBEDTLS_ENCRYPT) {
			err = mbedtls_camellia_setkey_enc((mbedtls_camellia_context *)ctx->cipher_ctx, ctx->key, ctx->key_bitlen);
		}
		else {
			err = mbedtls_camellia_setkey_dec((mbedtls_camellia_context *)ctx->cipher_ctx, ctx->key, ctx->key_bitlen);
		}
		COPY_MEM(ctx->nonce, ctx->IV, MBEDTLS_MAX_IV_LENGTH);
		break;
	#endif

	#ifdef MBEDTLS_ARIA_C
	case SYM_ARIA_128_ECB:
	case SYM_ARIA_192_ECB:
	case SYM_ARIA_256_ECB:
	case SYM_ARIA_128_CBC:
	case SYM_ARIA_192_CBC:
	case SYM_ARIA_256_CBC:
		mbedtls_aria_init((mbedtls_aria_context *)ctx->cipher_ctx);
		if (ctx->operation == MBEDTLS_ENCRYPT) {
			err = mbedtls_aria_setkey_enc((mbedtls_camellia_context *)ctx->cipher_ctx, ctx->key, ctx->key_bitlen);
		}
		else {
			err = mbedtls_aria_setkey_dec((mbedtls_camellia_context *)ctx->cipher_ctx, ctx->key, ctx->key_bitlen);
		}
		COPY_MEM(ctx->nonce, ctx->IV, MBEDTLS_MAX_IV_LENGTH);
		break;
#endif

	#ifdef MBEDTLS_CHACHA20_C
	case SYM_CHACHA20:
		err = mbedtls_chacha20_setkey((mbedtls_chacha20_context *)ctx->cipher_ctx, ctx->key);
		if (err) return err;
		counter = MBEDTLS_GET_UINT32_BE(ctx->IV, 12);
		err = mbedtls_chacha20_starts((mbedtls_chacha20_context *)ctx->cipher_ctx, ctx->IV, counter);
		break;
	#endif


	#ifdef MBEDTLS_CHACHAPOLY_C
	case SYM_CHACHA20_POLY1305:
		err = mbedtls_chachapoly_setkey((CHACHAPOLY_CTX *)ctx->cipher_ctx, ctx->key);
		if (err) return err;
		//COPY_MEM(ctx->nonce, ctx->IV, MBEDTLS_MAX_IV_LENGTH);
		// before start, we use part of the AAD as a dynamic_IV
		ctx->state = CRYPT_PORT_NEEDS_AAD;
		return CRYPT_OK;
	#endif

	}
	if (err) return err;


	ctx->state = CRYPT_PORT_READY;


	return CRYPT_OK;
}


/***********************************************************************
**
*/	static REBINT Crypt_Write(CRYPT_CTX *ctx, REBYTE *input, REBCNT len)
/*
***********************************************************************/
{
	REBINT  err;
	REBSER *bin;
	REBCNT  blk, olen, ofs;
	REBCNT unprocessed_free;
	REBCNT counter;
	REBYTE *end = input + len;

	if (len == 0) return 0;

	if (ctx->state == CRYPT_PORT_NEEDS_INIT) {
		err = Crypt_Init(ctx);
		if (err) return err;
	}

	blk = ctx->cipher_block_size;
	if (blk > MBEDTLS_MAX_BLOCK_LENGTH) return CRYPT_ERROR_BAD_BLOCK_SIZE;


	unprocessed_free = blk - ctx->unprocessed_len;
	if (len < unprocessed_free) {
		// input has not enough bytes to fill the block!
		COPY_MEM(ctx->unprocessed_data + ctx->unprocessed_len, input, len);
		ctx->unprocessed_len += len;
		if (ctx->unprocessed_len < blk)	return CRYPT_OK;
	}

	if (ctx->unprocessed_len > 0) {
		if (ctx->unprocessed_len > blk)
			return CRYPT_ERROR_BAD_UNPROCESSED_SIZE;
		// complete the block using the current input
		COPY_MEM(ctx->unprocessed_data + ctx->unprocessed_len, input, unprocessed_free);
		// complete the block
		Crypt_Crypt(ctx, ctx->unprocessed_data, blk, &olen);
		input += unprocessed_free;
		len -= unprocessed_free;
		// make the processed block empty again
		ctx->unprocessed_len = 0;
	}
	// input data could be already consumed on the unprocessed buffer
	if (len == 0) {
		return CRYPT_OK;
	}
	// test if input have enough data to do the block crypt
	if (len > blk) {
		// we have enough data to call crypt
		Crypt_Crypt(ctx, input, len, &olen);
		if (olen > len) return CRYPT_ERROR_BAD_PROCESSED_SIZE;
		len -= olen;
	}
	// test if there are some unprocessed data
	if (len > 0) {
		if (len > MBEDTLS_MAX_BLOCK_LENGTH) return CRYPT_ERROR_BAD_UNPROCESSED_SIZE;
		COPY_MEM(ctx->unprocessed_data, input, len);
		ctx->unprocessed_len = len;
	}
	// done
	return CRYPT_OK;
}


/***********************************************************************
**
*/	static int Crypt_Actor(REBVAL *ds, REBSER *port, REBCNT action)
/*
***********************************************************************/
{
	REBVAL *spec;
	REBVAL *state;
	REBVAL *data;
	REBVAL *arg1;
	REBVAL *arg2;
	REBSER *bin;
	REBSER *out;
	REBCNT  len;
	REBCNT  ofs, blk;
	REBINT  err;
	size_t olen = 0;
	CRYPT_CTX *ctx = NULL;

	Validate_Port(port, action);

	//printf("Crypt device action: %i\n", action);
	
	state = BLK_SKIP(port, STD_PORT_STATE);
	if (IS_HANDLE(state)) {
		if (VAL_HANDLE_TYPE(state) != SYM_CRYPT)
			Trap_Port(RE_INVALID_PORT, port, 0);
		ctx = (CRYPT_CTX *)VAL_HANDLE_CONTEXT_DATA(state);
	}

	if (action == A_OPEN) {
		if (ctx) {
			Trap_Port(RE_ALREADY_OPEN, port, 0);
		}
		if (!Crypt_Open(port)) {
			Trap_Port(RE_CANNOT_OPEN, port, 0);
			return R_FALSE;
		}
		return R_ARG1;
	}
	if (!ctx) {
		Trap_Port(RE_NOT_OPEN, port, 0);
		return R_NONE;
	}

	bin = ctx->buffer;

	switch (action) {
	case A_WRITE:
		//puts("write");
		arg1 = D_ARG(2);
		if (!IS_BINARY(arg1)) {
			Trap_Port(RE_FEATURE_NA, port, 0);
		}
		Crypt_Write(ctx, VAL_BIN_AT(arg1), VAL_LEN(arg1));
		break;
	case A_READ:
	case A_TAKE:
		//puts("read");
		len = BIN_LEN(bin);
		if (len > 0) {
			out = Make_Binary(len);
			COPY_MEM(BIN_DATA(out), BIN_DATA(bin), len);
			SET_BINARY(D_RET, out);
			BIN_LEN(out) = len;
			BIN_LEN(bin) = 0;
			return R_RET;
		}
		else return R_NONE;
		break;
	case A_UPDATE:
		//puts("update");
#ifdef MBEDTLS_CHACHAPOLY_C
		if (ctx->cipher_type == SYM_CHACHA20_POLY1305) {
			olen = SERIES_TAIL(bin);
			Expand_Series(bin, AT_TAIL, 16);
			SERIES_TAIL(bin) = olen; // reset the tail (above expand modifies it!)
			err = mbedtls_chachapoly_finish((mbedtls_chachapoly_context*)ctx->cipher_ctx, BIN_TAIL(bin));
			SERIES_TAIL(bin) += 16;
			ctx->state = CRYPT_PORT_NEEDS_AAD;
			return R_ARG1;
		}
#endif
		if (ctx->unprocessed_len > 0) {
			ofs = 0;
			blk = ctx->cipher_block_size;
			olen = SERIES_TAIL(bin);
			Expand_Series(bin, AT_TAIL, blk);
			// reset the tail (above expand modifies it!)
			SERIES_TAIL(bin) = olen;

			if (ctx->unprocessed_len > blk) abort();
			len = blk - ctx->unprocessed_len;
			// pad with zeros...
			CLEAR(ctx->unprocessed_data + ctx->unprocessed_len, len);

			Crypt_Crypt(ctx, ctx->unprocessed_data, blk, &olen);
			ctx->unprocessed_len = 0;
		}
		break;

	case A_CLOSE:
		if (ctx) {
			UNPROTECT_SERIES(ctx->buffer);
			Free_Hob(VAL_HANDLE_CTX(state));
			SET_NONE(state);
			ctx = NULL;
		}
		break;

	case A_OPENQ:
		return (ctx) ? R_TRUE : R_FALSE;

	case A_MODIFY:
		arg1 = D_ARG(2); // field
		arg2 = D_ARG(3); // value
		if (!IS_WORD(arg1)) break;
		switch (VAL_WORD_CANON(arg1)) {
//		case SYM_AAD:
//		case SYM_DATA:
//			if (IS_BINARY(arg2)) {
//			#ifdef MBEDTLS_CHACHAPOLY_C
//				err = mbedtls_chachapoly_update_aad((mbedtls_chachapoly_context *)ctx->cipher_ctx, VAL_BIN_AT(arg2), VAL_LEN(arg2));
//				if (!err) return R_TRUE;
//			#endif
//			}
//			return FALSE;
		case SYM_ALGORITHM:
			if (!init_crypt_algorithm(ctx, arg2)) return FALSE;
			break;
		case SYM_DIRECTION:
			if (!init_crypt_direction(ctx, arg2)) return R_FALSE;
			break;
		case SYM_KEY:
			if (!init_crypt_key(ctx, arg2)) return R_FALSE;
			break;
		case SYM_IV:
		case SYM_INIT_VECTOR:
	//		if (ctx->cipher_type == SYM_CHACHA20_POLY1305) {
	//			if (IS_BINARY(arg2)) {
	//				CLEAR(ctx->IV, MBEDTLS_MAX_IV_LENGTH);
	//				len = VAL_LEN(arg2);
	//				if (len > MBEDTLS_MAX_IV_LENGTH)
	//					len = MBEDTLS_MAX_IV_LENGTH;
	//				COPY_MEM(&ctx->IV, VAL_BIN_AT(arg2), len);
	//				return R_TRUE;
	//			}
	//			return R_FALSE;
	//		}
			if (!init_crypt_iv(ctx, arg2)) return R_FALSE;
			break;
		default:
			Trap1(RE_INVALID_ARG, arg1);
		}
		//err = mbedtls_cipher_setkey(&ctx->cipher, ctx->key, ctx->key_bitlen, ctx->operation);
		//if (err) return R_FALSE;
		//err = mbedtls_cipher_set_iv(&ctx->cipher, ctx->IV, 16);
		//if (err) return R_FALSE;
		//err = mbedtls_cipher_reset(&ctx->cipher);
		break;
	default:
		puts("not supported command");
		Trap_Action(REB_PORT, action);
	}
	return R_ARG1;
}


/***********************************************************************
**
*/	void Init_Crypt_Scheme(void)
/*
***********************************************************************/
{
	Register_Scheme(SYM_CRYPT, 0, Crypt_Actor);
}

/* DEFINE_DEV would normally be in os/dev-crypt.c but I'm not using it so it is here */
DEFINE_DEV(Dev_Crypt, "Crypt", 1, NULL, RDC_MAX, 0);

#endif //INCLUDE_MBEDTLS