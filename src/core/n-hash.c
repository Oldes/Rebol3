/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  Copyright 2012-2025 Rebol Open Source Contributors
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
**  Module:  n-hash.c
**  Summary: hashing values
**  Section: natives
**  Author:  Oldes
**  Notes:
**
***********************************************************************/

#include "sys-core.h"
#include "sys-hash.h"
#include "sys-xxhash.h"

//-----------------------------------------------------------------------------
REBCNT MurmurHash3_x86_32(const void* key, int len, REBCNT seed)
{
	const REBYTE* data = (const REBYTE*)key;
	const int nblocks = len / 4;

	REBCNT h1 = seed;

	//----------
	// body
	const REBCNT* blocks = (const REBCNT*)(data + nblocks * 4);

	for (int i = -nblocks; i; i++)
	{
		REBCNT k1 = getblock32(blocks, i);
		bmix(&h1, &k1);
	}

	//----------
	// tail
	const REBYTE* tail = (const REBYTE*)(data + nblocks * 4);

	REBCNT k1 = 0;

	switch (len & 3) {
	case 3: k1 ^= tail[2] << 16;
	case 2: k1 ^= tail[1] << 8;
	case 1: k1 ^= tail[0];
		k1 *= MURMUR_HASH_3_X86_32_C1;
		k1  = ROTL32(k1, 15);
		k1 *= MURMUR_HASH_3_X86_32_C2;
		h1 ^= k1;
	};

	//----------
	// finalization
	h1 ^= len;
	h1 = fmix32(h1);

	return h1;
}


/***********************************************************************
**
*/	REBNATIVE(hash)
/*
//	hash: native [
//		{Computes a hash value from any Rebol value. This number may change between different Rebol versions!}
//		value [any-type!]
//	]
***********************************************************************/
{
	DS_RET_INT(Hash_Value(D_ARG(1)) & 0xffffffff);
	return R_RET;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT Hash_Probe(REBCNT hash, REBCNT idx, REBCNT len)
/*
**		Calculates the index based on a quadratic probing approach.
**
**
***********************************************************************/
{
	//return ((hash % len) + idx) % len; // linear
	// computed in 64 bits, so the sequence is exact (same as the incremental
	// PROBE_START/PROBE_NEXT macros in sys-hash.h)
	return (REBCNT)(((REBU64)(hash % len) + idx + 3 * (REBU64)idx * idx) % len); // quadratic
	//return ((hash % len) + idx * (1 + (hash % (len - 1)))) % len; // double hashing
}

/***********************************************************************
**
*/	REBCNT Hash_Value(REBVAL* val)
/*
**		Return a case insensitive hash value for any Rebol value.
**
**
***********************************************************************/
{
	REBCNT ret;
	if (ANY_WORD(val))
		return VAL_WORD_CANON(val); //plain value seems to be faster than: fmix32(VAL_WORD_CANON(val));
	if (ANY_STR(val))
		return Hash_String_Value(val) ^ VAL_TYPE(val);
	if (ANY_BLOCK(val))
		return Hash_Block_Value(val);

	switch (VAL_TYPE(val)) {
	case REB_BINARY:
		return Hash_Binary(VAL_BIN_DATA(val), VAL_BIN_LEN(val));

	case REB_LOGIC:
		return 2 + VAL_LOGIC(val);

	case REB_INTEGER:
		return fmix32((REBCNT)(VAL_INT64(val) >> 32) ^ ((REBCNT)VAL_INT64(val)));

	case REB_DECIMAL:
	case REB_PERCENT: {
		// -0.0 is equal to 0.0 and all NaNs are equal, so normalize them first
		REBDEC d = VAL_DECIMAL(val) + 0.0; // -0.0 => 0.0
		REBI64 bits;
		if (d != d) bits = 0x7FF8000000000000LL; // any NaN => canonical NaN
		else memcpy(&bits, &d, sizeof(bits));
		return fmix32((REBCNT)(bits >> 32) ^ ((REBCNT)bits));
	}

	case REB_CHAR: // hashed always as lowercased
		if ((ret = VAL_CHAR(val)) < UNICODE_CASES) ret = LO_CASE(ret);
		return fmix32(ret); // (multiply and shift by 18 gave only 14 bits)

	case REB_MONEY: {
		// Deci values are unnormalized ($1 = $1.0 = $1.00, $0 = -$0), so hash
		// a canonical form: significand without trailing decimal zeros
		deci d = VAL_DECI(val);
		REBCNT m[3], q[3], k1, h1 = 0;
		REBINT e = d.e;
		REBU64 r;
		REBINT i;
		m[0] = d.m0; m[1] = d.m1; m[2] = d.m2;
		if ((m[0] | m[1] | m[2]) == 0) return 0; // any zero
		while (e < 127) {
			// divide the significand by 10, if it is exact
			for (r = 0, i = 2; i >= 0; i--) {
				r = (r << 32) | m[i];
				q[i] = (REBCNT)(r / 10);
				r %= 10;
			}
			if (r != 0) break;
			m[0] = q[0]; m[1] = q[1]; m[2] = q[2];
			e++;
		}
		k1 = m[0]; bmix(&h1, &k1);
		k1 = m[1]; bmix(&h1, &k1);
		k1 = m[2]; bmix(&h1, &k1);
		k1 = ((REBCNT)d.s << 8) | ((REBCNT)e & 0xff); bmix(&h1, &k1);
		return fmix32(h1);
	}

	case REB_TIME:
	case REB_DATE: {
		// Date without time is compared as midnight (see Cmp_Time)
		REBI64 t = VAL_TIME(val);
		if (t == NO_TIME) t = 0;
		ret = (REBCNT)(t ^ (t / SEC_SEC));
		if (IS_DATE(val)) {
			// Dates are stored in UTC and compared without the zone,
			// so the zone must not be part of the hash
			REBDAT date = VAL_DATE(val);
			date.date.zone = 0;
			ret ^= date.bits;
		}
		return ret;
	}

	case REB_TUPLE: {
		// Tuples are compared as zero padded (1.2.3 = 1.2.3.0),
		// so trailing zeros must not be part of the hash
		REBCNT len = VAL_TUPLE_LEN(val);
		REBYTE *tp = VAL_TUPLE(val);
		while (len > 0 && tp[len - 1] == 0) len--;
		return Hash_Binary(tp, len);
	}

	case REB_PAIR: {
		// Mix both coordinates (plain XOR made all NxN pairs hash to 0
		// and AxB same as BxA); adding 0.0 normalizes -0.0 to 0.0
		float f;
		REBCNT k1, h1 = 0;
		f = VAL_PAIR_X(val) + 0.0f; memcpy(&k1, &f, 4); bmix(&h1, &k1);
		f = VAL_PAIR_Y(val) + 0.0f; memcpy(&k1, &f, 4); bmix(&h1, &k1);
		return fmix32(h1);
	}

	case REB_TYPESET: {
		REBU64 ts = VAL_TYPESET(val);
		return fmix32((REBCNT)(ts >> 32) ^ ((REBCNT)ts));
	}

	case REB_DATATYPE:
		return fmix32(VAL_DATATYPE(val) + 1); // no need to compute CRC of the type's name

//	case REB_HANDLE:
//		return (REBCNT)VAL_HANDLE_I32(val); //TODO: this is not valid for context handles!
	}
	// all other types... 
	return fmix32(VAL_TYPE(val));
}

#define MAX_HASH_DEPTH 32 // max depth of nested blocks used for the hash

/***********************************************************************
**
*/	static REBCNT Hash_Block_Loop(REBVAL *block, REBSER **parents, REBCNT depth)
/*
**		Return a case insensitive hash value for the block.
**		Parents are series of the blocks being hashed (to detect circularity).
**		A nested block which is one of the parents, or which is too deep,
**		contributes only with its type.
**
***********************************************************************/
{
	REBCNT k1, h1 = 0, n;
	REBVAL *data = VAL_BLK_DATA(block);
	REBLEN len = VAL_LEN(block);

	parents[depth] = VAL_SERIES(block);

	k1 = VAL_TYPE(block);
	bmix(&h1, &k1);

	for (; NZ(VAL_TYPE(data)); data++) {
		if (ANY_BLOCK(data)) {
			k1 = VAL_TYPE(data);
			if (depth + 1 < MAX_HASH_DEPTH) {
				for (n = 0; n <= depth && parents[n] != VAL_SERIES(data); n++);
				if (n > depth) k1 = Hash_Block_Loop(data, parents, depth + 1);
			}
		}
		else k1 = Hash_Value(data);
		bmix(&h1, &k1);
	}
	h1 ^= len;
	h1 = fmix32(h1);

	return h1;
}

/***********************************************************************
**
*/	REBCNT Hash_Block_Value(REBVAL *block)
/*
**		Return a case insensitive hash value for the block.
**		Safe for circular blocks.
**
***********************************************************************/
{
	REBSER *parents[MAX_HASH_DEPTH];
	return Hash_Block_Loop(block, parents, 0);
}

/***********************************************************************
**
*/	static void Hash_Codepoint(REBCNT ch, REBCNT *hash, REBCNT *acc, REBCNT *n)
/*
**		Adds a lowercased codepoint, as UTF-8 bytes, to the string hash state.
**		Bytes are collected in `acc` and mixed by 4 (like Murmur3 blocks).
**
***********************************************************************/
{
	REBYTE b[4];
	REBCNT i, len;

	if (ch < UNICODE_CASES) ch = (REBCNT)LO_CASE(ch);
	if (ch < 0x80) {
		b[0] = (REBYTE)ch; len = 1;
	} else if (ch < 0x800) {
		b[0] = (REBYTE)(0xC0 | (ch >> 6));
		b[1] = (REBYTE)(0x80 | (ch & 0x3F)); len = 2;
	} else if (ch < 0x10000) {
		b[0] = (REBYTE)(0xE0 | (ch >> 12));
		b[1] = (REBYTE)(0x80 | ((ch >> 6) & 0x3F));
		b[2] = (REBYTE)(0x80 | (ch & 0x3F)); len = 3;
	} else {
		b[0] = (REBYTE)(0xF0 | (ch >> 18));
		b[1] = (REBYTE)(0x80 | ((ch >> 12) & 0x3F));
		b[2] = (REBYTE)(0x80 | ((ch >> 6) & 0x3F));
		b[3] = (REBYTE)(0x80 | (ch & 0x3F)); len = 4;
	}
	for (i = 0; i < len; i++) {
		*acc |= (REBCNT)b[i] << (8 * *n);
		if (++*n == 4) {
			bmix(hash, acc);
			*acc = 0;
			*n = 0;
		}
	}
}

/***********************************************************************
**
*/	REBCNT Hash_String_Value(REBVAL* val)
/*
**		Return a case insensitive hash value for the string value.
**		The hash is computed from the lowercased text encoded as UTF-8,
**		so it is the same for ASCII, UTF-8 and wide strings.
**		The length used is the number of codepoints.
**
***********************************************************************/
{
	const REBYTE *bin;
	REBLEN  len, i;
	REBLEN  count;
	REBCNT  hash = 0;
	REBCNT  acc = 0; // collected bytes
	REBCNT  n = 0;   // number of collected bytes

	if (BYTE_SIZE(VAL_SERIES(val))) {
		bin = VAL_BIN_DATA(val);
		len = VAL_TAIL(val) - VAL_INDEX(val);
		if (IS_UTF8_SERIES(VAL_SERIES(val))) {
			REBLEN bytes = len;
			for (count = 0; bytes > 0; bin++, count++) {
				// UTF8_Decode_Codepoint modifies bin and bytes
				Hash_Codepoint(UTF8_Decode_Codepoint(&bin, &bytes), &hash, &acc, &n);
			}
		}
		else {
			// Bytes (ASCII); 4 ASCII chars are mixed at once when aligned
			for (i = 0; i < len; ) {
				if (n == 0 && i + 4 <= len && ((bin[i] | bin[i+1] | bin[i+2] | bin[i+3]) & 0x80) == 0) {
					acc = (REBCNT)LO_CASE(bin[i])
						| ((REBCNT)LO_CASE(bin[i+1]) << 8)
						| ((REBCNT)LO_CASE(bin[i+2]) << 16)
						| ((REBCNT)LO_CASE(bin[i+3]) << 24);
					bmix(&hash, &acc);
					acc = 0;
					i += 4;
				}
				else Hash_Codepoint(bin[i++], &hash, &acc, &n);
			}
			count = len;
		}
	}
	else {
		REBUNI *uni = VAL_UNI_DATA(val);
		len = Val_Series_Len(val);
		for (i = 0; i < len; i++) Hash_Codepoint(uni[i], &hash, &acc, &n);
		count = len;
	}
	if (n > 0) bmix(&hash, &acc);
	hash ^= count;
	return fmix32(hash);
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT Hash_Binary(const REBYTE* bin, REBCNT len)
/*
**		Return a case sensitive hash value for the binary.
**
***********************************************************************/
{
	return MurmurHash3_x86_32(bin, len, 0);
	//return XXH64(bin, len, 0);

}

/***********************************************************************
**
*/	REBCNT Hash_Word(const REBYTE* str, REBLEN len)
/*
**		Return a case insensitive hash value for the UTF-8 encoded string.
**
***********************************************************************/
{
	REBCNT ch;
	REBCNT hash=0;
	REBLEN bytes;

	if (len == UNKNOWN) len = LEN_BYTES(str);
	bytes = len;

	for (; bytes > 0; str++) {
		ch = UTF8_Decode_Codepoint(&str, &bytes); // mods str, ulen
		if (!ch) Trap0(RE_INVALID_CHARS);
		if (ch < UNICODE_CASES) ch = LO_CASE(ch);
		bmix(&hash, &ch);
	}
	hash ^= len;
	hash = fmix32(hash);
	return hash;
}

/***********************************************************************
**
*/	REBSER* Make_Hash_Array(REBCNT len)
/*
***********************************************************************/
{
	REBCNT n;
	REBSER* ser;

	n = Get_Hash_Prime(len * 2); // best when 2X # of keys
	if (!n) Trap_Num(RE_SIZE_LIMIT, len);

	ser = Make_Series(n + 1, sizeof(REBCNT), FALSE);
	LABEL_SERIES(ser, "make hash array");
	//No need to clear the series, because Make_Series guarantees completely cleared memory.
	//Clear_Series(ser);
	ser->tail = n;

	return ser;
}


/***********************************************************************
**
*/	REBSER* Hash_Block(REBVAL* block, REBCNT cased)
/*
**		Hash ALL values of a block. Return hash array series.
**		Used for SET logic (unique, union, etc.)
**
**		Note: hash array contents (indexes) are 1-based!
**
***********************************************************************/
{
	REBCNT n;
	REBCNT key;
	REBSER* hser;
	REBCNT* hashes;
	REBSER* series = VAL_SERIES(block);

	// Create the hash array (integer indexes):
	hser = Make_Hash_Array(VAL_LEN(block));
	hashes = (REBCNT*)hser->data;

	for (n = VAL_INDEX(block); n < series->tail; n++) {
		key = Find_Key(series, hser, BLK_SKIP(series, n), 1, cased, 0);
		hashes[key] = n + 1;
	}

	return hser;
}