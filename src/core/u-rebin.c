/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2026 Rebol Open Source Contributors
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
**  Module:  u-rebin.c
**  Summary: decoder of the Rebin (binary serialised Rebol values) format
**  Section: utility
**  Author:  Oldes
**  Notes:
**    Rebin holds already scanned (loaded, but not bound) Rebol values.
**    Its main purpose is to avoid the lexical scan of the boot code.
**
**    Format (version 1):
**
**      header:   "REBIN" version:u8 types:u8 flags:u8
**      symbols:  count:varint { length:varint utf8-bytes }
**      values:   count:varint { value }
**
**      value:    tag:u8 [flags:u8] payload
**                bits 0-6 of the tag are the datatype (REB_* number)
**                bit  7   means, that the flags byte follows the tag
**
**      flags:    bit 0    new-line before the value
**                The other bits are reserved (for example for protection)
**                and data using them are refused.
**
**      payload:  unset none               (nothing)
**                logic                    1 byte (0 or 1)
**                integer time             varint (64-bit two's complement)
**                decimal percent          8 bytes (IEEE 754, little endian)
**                money                    12 bytes (as `to binary! money`)
**                char                     varint (codepoint)
**                pair                     2x 4 bytes (float, little endian)
**                tuple                    length:u8 bytes
**                date                     year:varint month:u8 day:u8 has-time:u8
**                                         and when has-time is 1 (else 0):
**                                         time:varint zone:u8
**                                         The date and time are local (as
**                                         in the source), time is as above
**                                         and zone is a signed number of
**                                         15 minute steps.
**                datatype                 type:u8 (REB_* number)
**                typeset                  bits:varint (bit n is REB_* type n)
**                binary and any string    length:varint bytes
**                bitset                   not:u8 length:varint bytes
**                                         not is 1 for a complemented
**                                         bitset (else 0); the bytes are
**                                         the stored ones in both cases
**                image                    width:varint height:varint
**                                         width * height * 4 bytes (RGBA,
**                                         as `to binary! image`)
**                vector                   type:u8 rows:varint count:varint
**                                         count * width bytes (little endian)
**                                         type is VTSI08..VTSF64 (not 8 or 9),
**                                         count must be rows * columns
**                any block and hash       count:varint { value }
**                map                      count:varint { key value }
**                                         count is the number of values
**                                         (twice the number of pairs)
**                any word                 symbol-index:varint
**
**    varint is an unsigned LEB128 number (see sys-leb128.h), the same as
**    Bincode's ENCODEDU64. All series are at the head.
**    The symbols are interned in the order of the table, so the words
**    get the same symbol numbers as if the source had been scanned.
**
**    The datatypes are stored as the internal REB_* numbers, so the data
**    may be decoded only by a build with the same datatypes as the one,
**    for which the data were made. The `types` byte of the header holds
**    the number of datatypes (REB_MAX) and data with another number are
**    refused. Note that it does not detect reordered datatypes!
**
**    The input is validated, so not valid data are refused instead of
**    making not valid values. The only exception are the characters of
**    words, which are checked just to be a valid UTF-8 text.
**
***********************************************************************/

#include "sys-core.h"
#include "sys-leb128.h"
#include "sys-deci-funcs.h" // binary_to_deci
#ifdef INCLUDE_REBIN_CODEC

#define REBIN_VERSION    1
#define REBIN_HEAD_SIZE  8

#define RBT_MASK         0x7F // datatype bits of the tag
#define RBT_FLAGS        0x80 // the tag is followed by the flags byte

// Value flags (stored only when some is set)
#define RBF_LINE         0x01 // new-line before the value
#define RBF_KNOWN        (RBF_LINE)

typedef struct Rebin_State {
	const REBYTE *cp;    // current input position
	const REBYTE *end;   // end of the input
	REBCNT *syms;        // symbol table index -> symbol number
	REBCNT  nsyms;       // number of symbols in the table
} REBIN_STATE;

// Number of bytes which were not read yet
#define REMAINS(rs) ((REBCNT)((rs)->end - (rs)->cp))

static REBSER *Decode_Block(REBIN_STATE *rs);


/***********************************************************************
**
*/	static REBOOL Is_Rebin(const REBYTE *data, REBCNT len)
/*
**		Check the header of the data.
**
***********************************************************************/
{
	return (
		len >= REBIN_HEAD_SIZE
		&& data[0] == 'R' && data[1] == 'E' && data[2] == 'B'
		&& data[3] == 'I' && data[4] == 'N'
		&& data[5] == REBIN_VERSION
		&& data[6] == REB_MAX
	);
}


/***********************************************************************
**
*/	static REBOOL Read_Varint(REBIN_STATE *rs, REBU64 *out)
/*
**		Read an unsigned LEB128 number.
**
***********************************************************************/
{
	REBCNT n = Read_LEB128(rs->cp, rs->end, out);
	rs->cp += n;
	return n != 0;
}


/***********************************************************************
**
*/	static REBOOL Read_Length(REBIN_STATE *rs, REBCNT *out)
/*
**		Read a number of bytes or items which follow in the input.
**		Each item takes at least one byte, so the number can never
**		be higher than the number of remaining bytes.
**
***********************************************************************/
{
	REBU64 num;

	if (!Read_Varint(rs, &num)) return FALSE;
	if (num > REMAINS(rs)) return FALSE;
	*out = (REBCNT)num;
	return TRUE;
}


/***********************************************************************
**
*/	static REBOOL Is_Text(const REBYTE *bp, REBCNT len, REBOOL *ascii)
/*
**		Returns TRUE when the bytes are a valid UTF-8 text. Strings
**		and words with a not valid encoding must never be created,
**		because the rest of the system expects them to be correct.
**
***********************************************************************/
{
	const REBYTE *ep = bp + len;
	REBFLG surrogates;

	// Most of the text is ASCII only, which needs no other validation.
	while (bp < ep && *bp < 0x80) bp++;

	*ascii = (bp == ep);
	if (*ascii) return TRUE;

	// Validate just the rest (starting with the first not ASCII byte).
	return (!UTF8_Check(bp, (REBCNT)(ep - bp), &surrogates) && !surrogates);
}


/***********************************************************************
**
*/	static REBOOL Is_Money_Binary(const REBYTE *bp)
/*
**		Returns TRUE when the 12 bytes are a valid money value.
**		It is the check of the binary_to_deci function, which
**		throws an error instead.
**
***********************************************************************/
{
	REBCNT m2 = ((REBCNT)(bp[1] & 0x7F) << 16) | ((REBCNT)bp[2] << 8) | bp[3];
	REBCNT m1 = READ_BE_4(bp + 4);
	REBCNT m0 = READ_BE_4(bp + 8);

	if (m2 > 5421010u) return FALSE;
	if (m2 < 5421010u) return TRUE;
	if (m1 > 3704098002u) return FALSE;
	if (m1 < 3704098002u) return TRUE;
	return m0 <= 3825205247u;
}


/***********************************************************************
**
*/	static REBOOL Decode_Date(REBIN_STATE *rs, REBVAL *value)
/*
**		Decode a date, which is stored as it would be written in
**		the source (local date and time with a zone). The same
**		limits are used as in the Scan_Date function.
**
***********************************************************************/
{
	REBU64 year;
	REBU64 num;
	REBCNT month;
	REBCNT day;
	REBI64 time = NO_TIME;
	REBINT zone = 0;

	if (!Read_Varint(rs, &year) || year > MAX_YEAR) return FALSE;

	if (REMAINS(rs) < 3) return FALSE;
	month = rs->cp[0];
	day   = rs->cp[1];
	if (month < 1 || month > 12) return FALSE;
	if (day < 1 || day > Month_Lengths[month - 1]) return FALSE;
	if (month == 2 && day == 29) {
		// Only in a leap year
		if ((year % 4) != 0 || ((year % 100) == 0 && (year % 400) != 0)) return FALSE;
	}

	switch (rs->cp[2]) {
	case 0: // date only
		rs->cp += 3;
		break;
	case 1: // date with time and zone
		rs->cp += 3;
		if (!Read_Varint(rs, &num)) return FALSE;
		time = (REBI64)num;
		if (time < 0 || time >= TIME_SEC(24 * 60 * 60)) return FALSE;
		if (rs->cp >= rs->end) return FALSE;
		zone = (signed char)*rs->cp++;
		if (zone < -MAX_ZONE || zone > MAX_ZONE) return FALSE;
		break;
	default:
		return FALSE;
	}

	Set_Date_UTC(value, (REBINT)year, month, day, time, zone);
	return TRUE;
}


/***********************************************************************
**
*/	static REBOOL Decode_Image(REBIN_STATE *rs, REBVAL *value)
/*
**		Decode an image. The same as for example:
**		#(image! 2x1 #{FF000000FF00} #{80FF})
**
***********************************************************************/
{
	REBU64 w;
	REBU64 h;
	REBSER *img;

	if (!Read_Varint(rs, &w) || !Read_Varint(rs, &h)) return FALSE;
	// The size limit is the one of the Make_Image function.
	if (w > 0xFFFF || h > 0xFFFF) return FALSE;
	if (w * h * 4 > REMAINS(rs)) return FALSE;

	img = Make_Image((REBCNT)w, (REBCNT)h, FALSE);
	if (!img) return FALSE;
	Bin_To_RGBA(IMG_DATA(img), (REBCNT)(w * h), (REBYTE *)rs->cp, (REBINT)(w * h), FALSE);
	rs->cp += w * h * 4;
	SET_IMAGE(value, img);
	return TRUE;
}


/***********************************************************************
**
*/	static REBOOL Decode_Vector(REBIN_STATE *rs, REBVAL *value)
/*
**		Decode a vector with numeric elements. The same as for
**		example: #(uint8! 3x2 #{010203040506})
**
**		Vectors of structs are not supported (there is no struct!).
**
***********************************************************************/
{
	REBCNT vtype;
	REBU64 rows;
	REBCNT count;
	REBCNT wide;
	REBYTE *data;

	if (rs->cp >= rs->end) return FALSE;
	vtype = *rs->cp++;
	// VTSF08 and VTSF16 are not used.
	if (vtype >= VT_MAX || vtype == VTSF08 || vtype == VTSF16) return FALSE;
	wide = VECT_WIDE(vtype);

	if (!Read_Varint(rs, &rows) || !Read_Length(rs, &count)) return FALSE;
	// All rows must be full (an empty vector is one empty row).
	if (rows < 1 || rows > VECT_INFO_ROWS_MAX) return FALSE;
	if (count == 0 ? rows != 1 : (count % rows) != 0) return FALSE;
	if ((REBU64)count * wide > REMAINS(rs)) return FALSE;

	if (!Make_Vector(value, vtype, count / (REBCNT)rows, (REBINT)rows)) return FALSE;
	data = VAL_VEC_HEAD(value);
	memcpy(data, rs->cp, count * wide);
	rs->cp += count * wide;
#ifdef ENDIAN_BIG
	if (wide > 1) Swap_Endian(data, count * wide, wide);
#endif
	return TRUE;
}


/***********************************************************************
**
*/	static REBOOL Decode_Value(REBIN_STATE *rs, REBVAL *value)
/*
**		Decode a single value into the given (cleared) location.
**
***********************************************************************/
{
	REBCNT  tag;
	REBCNT  type;
	REBCNT  flags = 0;
	REBCNT  is_not;
	REBU64  num;
	REBCNT  len;
	REBSER *ser;
	REBDEC  dec;
	float   x, y;
	u32     bits;
	REBOOL  ascii;

	if (rs->cp >= rs->end) return FALSE;
	tag  = *rs->cp++;
	type = tag & RBT_MASK;

	if (tag & RBT_FLAGS) {
		if (rs->cp >= rs->end) return FALSE;
		flags = *rs->cp++;
		// Not known flag must not be silently ignored.
		if (flags & ~RBF_KNOWN) return FALSE;
	}

	switch (type) {

	case REB_UNSET:
	case REB_NONE:
		VAL_SET(value, type);
		break;

	case REB_LOGIC:
		if (rs->cp >= rs->end || *rs->cp > 1) return FALSE;
		SET_LOGIC(value, *rs->cp++);
		break;

	case REB_INTEGER:
		if (!Read_Varint(rs, &num)) return FALSE;
		SET_INTEGER(value, (REBI64)num);
		break;

	case REB_DECIMAL:
	case REB_PERCENT:
		if (REMAINS(rs) < 8) return FALSE;
		num = READ_LE_8(rs->cp);
		rs->cp += 8;
		memcpy(&dec, &num, sizeof(dec));
		VAL_SET(value, type);
		VAL_DECIMAL(value) = dec;
		break;

	case REB_MONEY:
		if (REMAINS(rs) < 12 || !Is_Money_Binary(rs->cp)) return FALSE;
		SET_MONEY(value, binary_to_deci(rs->cp));
		rs->cp += 12;
		break;

	case REB_CHAR:
		if (!Read_Varint(rs, &num) || num > MAX_UNI || IS_SURROGATE(num)) return FALSE;
		SET_CHAR(value, num);
		break;

	case REB_PAIR:
		if (REMAINS(rs) < 8) return FALSE;
		bits = READ_LE_4(rs->cp);     memcpy(&x, &bits, sizeof(x));
		bits = READ_LE_4(rs->cp + 4); memcpy(&y, &bits, sizeof(y));
		rs->cp += 8;
		SET_PAIR(value, x, y);
		break;

	case REB_TUPLE:
		if (rs->cp >= rs->end) return FALSE;
		len = *rs->cp++;
		if (len > MAX_TUPLE || len > REMAINS(rs)) return FALSE;
		VAL_SET(value, REB_TUPLE);
		VAL_TUPLE_LEN(value) = len;
		CLEAR(VAL_TUPLE(value), MAX_TUPLE);
		memcpy(VAL_TUPLE(value), rs->cp, len);
		rs->cp += len;
		break;

	case REB_TIME:
		if (!Read_Varint(rs, &num)) return FALSE;
		VAL_SET(value, REB_TIME);
		VAL_TIME(value) = (REBI64)num;
		break;

	case REB_DATE:
		if (!Decode_Date(rs, value)) return FALSE;
		break;

	case REB_DATATYPE:
		// The same as: #(integer!) (see Construct_Value)
		if (rs->cp >= rs->end || *rs->cp >= REB_MAX) return FALSE;
		VAL_SET(value, REB_DATATYPE);
		VAL_DATATYPE(value) = *rs->cp++;
		VAL_TYPE_SPEC(value) = 0;
		break;

	case REB_TYPESET:
		// Only existing datatypes may be in the set.
		if (!Read_Varint(rs, &num)) return FALSE;
		if (REB_MAX < 64 && (num >> REB_MAX) != 0) return FALSE;
		VAL_SET(value, REB_TYPESET);
		VAL_TYPESET(value) = num;
		break;

	case REB_BINARY:
		if (!Read_Length(rs, &len)) return FALSE;
		ser = Copy_Bytes(rs->cp, len);
		rs->cp += len;
		Set_Series(REB_BINARY, value, ser);
		break;

	case REB_BITSET:
		// The same as: #(bitset! #{...}) or #(bitset! not #{...})
		if (rs->cp >= rs->end || *rs->cp > 1) return FALSE;
		is_not = *rs->cp++;
		if (!Read_Length(rs, &len)) return FALSE;
		ser = Copy_Bytes(rs->cp, len);
		rs->cp += len;
		Set_Series(REB_BITSET, value, ser);
		BITS_NOT(ser) = is_not;
		break;

	case REB_STRING:
	case REB_FILE:
	case REB_EMAIL:
	case REB_REF:
	case REB_URL:
	case REB_TAG:
		if (!Read_Length(rs, &len)) return FALSE;
		if (!Is_Text(rs->cp, len, &ascii)) return FALSE;
		ser = Copy_Bytes(rs->cp, len);
		if (!ascii) UTF8_SERIES(ser);
		rs->cp += len;
		Set_Series(type, value, ser);
		break;

	case REB_BLOCK:
	case REB_PAREN:
	case REB_PATH:
	case REB_SET_PATH:
	case REB_GET_PATH:
	case REB_LIT_PATH:
	case REB_HASH: // its hash table is made when needed
		ser = Decode_Block(rs);
		if (!ser) return FALSE;
		Set_Series(type, value, ser);
		break;

	case REB_VECTOR:
		if (!Decode_Vector(rs, value)) return FALSE;
		break;

	case REB_IMAGE:
		if (!Decode_Image(rs, value)) return FALSE;
		break;

	case REB_MAP:
		// The same as the map literal: #[key value ...]
		ser = Decode_Block(rs);
		if (!ser) return FALSE;
		Set_Block(value, ser);
		if (!MT_Map(value, value, 0)) return FALSE; // odd number of values
		break;

	case REB_WORD:
	case REB_SET_WORD:
	case REB_GET_WORD:
	case REB_LIT_WORD:
	case REB_REFINEMENT:
	case REB_ISSUE:
		if (!Read_Varint(rs, &num) || num >= rs->nsyms) return FALSE;
		VAL_SET(value, type);
		VAL_WORD_SYM(value) = rs->syms[num];
		VAL_WORD_INDEX(value) = 0;
		VAL_WORD_FRAME(value) = 0;
		break;

	default:
		// Datatype which is not supported (yet), or not valid at all.
		return FALSE;
	}

	// All cases above set the value header, so the flags must be applied here.
	if (flags & RBF_LINE) VAL_SET_LINE(value);

	return TRUE;
}


/***********************************************************************
**
*/	static REBSER *Decode_Block(REBIN_STATE *rs)
/*
**		Decode counted values into a new block of the exact size.
**		Returns NULL on error.
**
***********************************************************************/
{
	REBSER *block;
	REBVAL *value;
	REBCNT  count;
	REBCNT  n;

	CHECK_STACK(&count);

	if (!Read_Length(rs, &count)) return NULL;

	block = Make_Block(count);
	LABEL_SERIES(block, "rebin block");

	// The tail is moved with each value, so the block is always valid.
	for (n = 0; n < count; n++) {
		value = BLK_SKIP(block, n);
		if (!Decode_Value(rs, value)) {
			SET_END(value);
			return NULL;
		}
		block->tail = n + 1;
	}
	BLK_TERM(block);

	return block;
}


/***********************************************************************
**
*/	REBSER *Decode_Rebin(const REBYTE *data, REBCNT len)
/*
**		Decode Rebin data into a block of (not bound) values.
**		Returns NULL when the data are not valid.
**
**		This is the equivalent of the Scan_Source function.
**
***********************************************************************/
{
	REBIN_STATE rs;
	REBSER *syms;
	REBSER *block = NULL;
	REBCNT  size;
	REBCNT  n;
	REBOOL  ascii;

	// The datatype must fit into the tag byte together with the flags bit!
	STATIC_ASSERT(REB_MAX <= RBT_MASK + 1);

	if (!Is_Rebin(data, len)) return NULL;

	rs.cp  = data + REBIN_HEAD_SIZE;
	rs.end = data + len;

	if (!Read_Length(&rs, &rs.nsyms)) return NULL;

	// Intern all symbols first (in the order of the table!)
	syms = Make_Series(rs.nsyms + 1, sizeof(REBCNT), FALSE);
	rs.syms = (REBCNT *)syms->data;

	for (n = 0; n < rs.nsyms; n++) {
		// Make_Word takes zero length as a request to count it itself.
		if (!Read_Length(&rs, &size) || size == 0) goto done;
		if (!Is_Text(rs.cp, size, &ascii)) goto done;
		rs.syms[n] = Make_Word(rs.cp, size);
		rs.cp += size;
	}

	block = Decode_Block(&rs);

	// Nothing may follow the values.
	if (block && rs.cp != rs.end) block = NULL;

done:
	Free_Series(syms);
	return block;
}


/***********************************************************************
**
*/	REBINT Codec_REBIN(REBCDI *codi)
/*
***********************************************************************/
{
	REBSER *block;

	codi->error = 0;

	if (codi->action == CODI_IDENTIFY) {
		if (!Is_Rebin(codi->data, codi->len))
			codi->error = CODI_ERR_SIGNATURE;
		return CODI_CHECK; // error code is inverted result
	}

	if (codi->action == CODI_DECODE) {
		block = Decode_Rebin(codi->data, codi->len);
		if (!block) {
			codi->error = CODI_ERR_BAD_DATA;
			return CODI_ERROR;
		}
		codi->other = (void *)block;
		return CODI_BLOCK;
	}

	codi->error = CODI_ERR_NA;
	return CODI_ERROR;
}


/***********************************************************************
**
*/	void Init_REBIN_Codec(void)
/*
***********************************************************************/
{
	Register_Codec("rebin", Codec_REBIN);
}

#endif