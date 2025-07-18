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
**  Module:  s-unicode.c
**  Summary: unicode support functions
**  Section: strings
**  Author:  Carl Sassenrath
**  Notes:
**    The top part of this code is from Unicode Inc. The second
**    part was added by REBOL Technologies.
**
***********************************************************************/


/*
 * Copyright 2001-2004 Unicode, Inc.
 * 
 * Disclaimer
 * 
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 * 
 * Limitations on Rights to Redistribute This Code
 * 
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

	Conversions between UTF32, UTF-16, and UTF-8.  Header file.

	Several funtions are included here, forming a complete set of
	conversions between the three formats.  UTF-7 is not included
	here, but is handled in a separate source file.

	Each of these routines takes pointers to input buffers and output
	buffers.  The input buffers are const.

	Each routine converts the text between *sourceStart and sourceEnd,
	putting the result into the buffer between *targetStart and
	targetEnd. Note: the end pointers are *after* the last item: e.g. 
	*(sourceEnd - 1) is the last item.

	The return result indicates whether the conversion was successful,
	and if not, whether the problem was in the source or target buffers.
	(Only the first encountered problem is indicated.)

	After the conversion, *sourceStart and *targetStart are both
	updated to point to the end of last text successfully converted in
	the respective buffers.

	Input parameters:
	sourceStart - pointer to a pointer to the source buffer.
		The contents of this are modified on return so that
		it points at the next thing to be converted.
	targetStart - similarly, pointer to pointer to the target buffer.
	sourceEnd, targetEnd - respectively pointers to the ends of the
		two buffers, for overflow checking only.

	These conversion functions take a ConversionFlags argument. When this
	flag is set to strict, both irregular sequences and isolated surrogates
	will cause an error.  When the flag is set to lenient, both irregular
	sequences and isolated surrogates are converted.

	Whether the flag is strict or lenient, all illegal sequences will cause
	an error return. This includes sequences such as: <F4 90 80 80>, <C0 80>,
	or <A0> in UTF-8, and values above 0x10FFFF in UTF-32. Conformant code
	must check for illegal sequences.

	When the flag is set to lenient, characters over 0x10FFFF are converted
	to the replacement character; otherwise (when the flag is set to strict)
	they constitute an error.

	Output parameters:
	The value "sourceIllegal" is returned from some routines if the input
	sequence is malformed.  When "sourceIllegal" is returned, the source
	value will point to the illegal value that caused the problem. E.g.,
	in UTF-8 when a sequence is malformed, it points to the start of the
	malformed sequence.  

	Author: Mark E. Davis, 1994.
	Rev History: Rick McGowan, fixes & updates May 2001.
		 Fixes & updates, Sept 2001.

------------------------------------------------------------------------ */

#include "sys-core.h"
#include <wchar.h>

#define UNI_SUR_HIGH_START  (REBU32)0xD800
#define UNI_SUR_HIGH_END    (REBU32)0xDBFF
#define UNI_SUR_LOW_START   (REBU32)0xDC00
#define UNI_SUR_LOW_END     (REBU32)0xDFFF

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

static const REBYTE utf8d[] = {
  // The first part of the table maps bytes to character classes that
  // to reduce the size of the transition table and create bitmasks.
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x00 - 0x1F
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x20 - 0x2F
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x40 - 0x4F
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x60 - 0x6F
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 0x80 - 0x8F
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // 0xA0 - 0xAF
   8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // 0xC0 - 0xCF
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8, // 0xE0 - 0xEF

  // The second part is a transition table that maps a combination
  // of a state of the automaton and a character class to a state.
   0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12, 
};

static const REBYTE utf8d_class_to_size[] = {
	[0] = 1, // ASCII
	[1] = 1, // Continuation
	[2] = 2, // 2-byte lead
	[3] = 3, // 3-byte lead
	[4] = 4, // 4-byte lead
	[5] = 1, // Invalid
	[6] = 1, // Invalid
	[7] = 1, // Continuation
	[8] = 1, // Continuation
	[9] = 1, // Invalid
	[10] = 3,// 3-byte lead
	[11] = 4 // 4-byte lead
};

static REBYTE const u8_length[] = {
	// 0 1 2 3 4 5 6 7 8 9 A B C D E F
	   1,1,1,1,1,1,1,1,0,0,0,0,2,2,3,4
};


// Helper to write a UTF-32 code point with specified endianness
FORCE_INLINE
static void write_u32(REBYTE *dst, REBU32 codepoint, int is_little_endian) {
	if (is_little_endian) {
		dst[0] = (REBYTE)(codepoint & 0xFF);
		dst[1] = (REBYTE)((codepoint >> 8) & 0xFF);
		dst[2] = (REBYTE)((codepoint >> 16) & 0xFF);
		dst[3] = (REBYTE)((codepoint >> 24) & 0xFF);
	}
	else {
		dst[0] = (REBYTE)((codepoint >> 24) & 0xFF);
		dst[1] = (REBYTE)((codepoint >> 16) & 0xFF);
		dst[2] = (REBYTE)((codepoint >> 8) & 0xFF);
		dst[3] = (REBYTE)(codepoint & 0xFF);
	}
}
// Helper to read a 16-bit code unit with endianness
FORCE_INLINE
static REBU16 read_u16(const REBYTE *src, int is_little_endian) {
	if (is_little_endian)
		return (REBU16)src[0] | ((REBU16)src[1] << 8);
	else
		return ((REBU16)src[0] << 8) | (REBU16)src[1];
}

// Helper to read a 32-bit code unit with endianness
FORCE_INLINE
static REBU32 read_u32(const REBYTE *src, int is_little_endian) {
	if (is_little_endian)
		return (REBU32)src[0] | ((REBU32)src[1] << 8) |
		((REBU32)src[2] << 16) | ((REBU32)src[3] << 24);
	else
		return ((REBU32)src[0] << 24) | ((REBU32)src[1] << 16) |
		((REBU32)src[2] << 8) | (REBU32)src[3];
}


FORCE_INLINE
/***********************************************************************
**
*/	REBCNT UTF8_Codepoint_Size(REBU32 codepoint)
/*
**		Returns the size of the given codepoint in bytes.
**
***********************************************************************/
{
	if (codepoint <= 0x7F) return 1;
	if (codepoint <= 0x7FF) return 2;
	if (codepoint <= 0xFFFF) return 3;
	return 4;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT UTF8_Next_Char_Size(const REBYTE *str, REBLEN index)
/*
**		Returns the size of the current UTF8 char in bytes
**
***********************************************************************/
{
	REBYTE c = str[index];
	REBYTE class = utf8d[c];
	return utf8d_class_to_size[class];

//	if ((c & 0x80) == 0) return 1;     // ASCII (0xxxxxxx)
//	if ((c & 0xE0) == 0xC0) return 2;   // 2-byte sequence (110xxxxx)
//	if ((c & 0xF0) == 0xE0) return 3;   // 3-byte sequence (1110xxxx)
//	if ((c & 0xF8) == 0xF0) return 4;   // 4-byte sequence (11110xxx)
//	return 1; // Fallback for invalid/continuation bytes
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT UTF8_Decode_Step(REBCNT* state, REBCNT* codep, REBCNT byte)
/*
***********************************************************************/
{
  REBCNT type = utf8d[byte];

  *codep = (*state != UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state + type];
  return *state;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT UTF8_Prev_Char_Position(const REBYTE *str, REBLEN index)
/*
**		Returns the index of the previous UTF-8 character
**
***********************************************************************/
{
	do { index--; } while (index > 0 && (str[index] & 0xC0) == 0x80);
	return index;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBLEN UTF8_Prev_Char_Size(const REBYTE *str, REBLEN index)
/*
**		Returns the index of the previous UTF-8 character
**
***********************************************************************/
{
	REBLEN size = 0;
	do { index--; size++; } while (index > 0 && (str[index] & 0xC0) == 0x80);
	return size;
}

/***********************************************************************
**
*/	REBCNT UTF8_Index_To_Position(const REBYTE *str, REBLEN index)
/*
**		Returns number of codepoints which UTF8 string have at the given position
**
***********************************************************************/
{
	REBCNT pos = 0;
	const REBYTE *end = str + index;
	while (*str && str < end) {
		pos += (*str++ & 0xC0) != 0x80;
	}
	return pos;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT UTF8_Skip_Forward(const REBYTE *str, REBLEN chars)
/*
**		Return number of bytes needed for given number of chars forward.
**
***********************************************************************/
{
	REBLEN index = 0;
	while (chars-- > 0 && str[index]) {
		index += UTF8_Next_Char_Size(str, index);
	}
	return index;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT UTF8_Skip(const REBSER *ser, REBCNT index, REBINT chars)
/*
**		Return position in series after skipping number of chars forward or reverse.
**
***********************************************************************/
{
	REBYTE *head = BIN_HEAD(ser);
	if (chars > 0) {
		while (chars-- > 0 && head[index]) {
			index += UTF8_Next_Char_Size(head, index);
		}
	}
	else {
		while (chars++ < 0 && index > 0) {
			index -= UTF8_Prev_Char_Size(head, index);
		}
	}
	return index;
}


FORCE_INLINE
/***********************************************************************
**
*/	REBCNT UTF8_Validate_Index(const REBYTE *str, REBLEN index)
/*
**		Returns the index of the previous UTF-8 character
**
***********************************************************************/
{
	do { index--; } while (index > 0 && (str[index] & 0xC0) == 0x80);
	return index;
}


/***********************************************************************
**
*/	REBLEN UTF8_Bytes_For_Char_Count(const REBYTE *str, REBLEN tail, REBLEN len)
/*
**		Returns number of bytes consumed by given number of UTF8 chars in a string
**
***********************************************************************/
{
	REBLEN pos = 0;
	while (pos < tail && len-- > 0) {
		pos += UTF8_Next_Char_Size(str, pos);
	}
	return pos;
}

/***********************************************************************
**
*/	REBLEN UTF8_Bytes_For_Char_Count_Back(const REBYTE *str, REBLEN index, REBLEN num)
/*
**		Returns number of bytes consumed by given number of UTF8 chars in a string
**
***********************************************************************/
{
	REBLEN size;
	while (index > 0 && num-- > 0) {
		size = UTF8_Prev_Char_Size(str, index);
		ASSERT1(size <= index, RP_BAD_SIZE);
		index -= size;
	}
	return index;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT UTF8_Get_Codepoint(const REBYTE *src)
/*
**		Gets a single UTF8 code-point (to 32 bit).
**
***********************************************************************/
{
	REBCNT codepoint = 0;
	REBCNT state = 0;

	for (; *src; ++src) {
		// read bytes until codepoint is not complete or invalid...
		if (UTF8_Decode_Step(&state, &codepoint, *src)) {
			if (state != UTF8_REJECT) continue; // not yet complete
			break;
		}
		break; // only one codepoint
	}
	if (state != UTF8_ACCEPT) codepoint = UNI_REPLACEMENT_CHAR;
	return codepoint;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBSER *UTF8_Replace_Codepoint(REBSER *ser, REBLEN index, REBU32 codepoint)
/*
**		Replace codepoint at given index with a new one.
**
***********************************************************************/
{
	REBLEN s1 = UTF8_Next_Char_Size(STR_HEAD(ser), index);
	REBLEN s2 = UTF8_Codepoint_Size(codepoint);
	if (s2 > s1)
		Expand_Series(ser, index, s2 - s1);
	else if (s2 < s1)
		Remove_Series(ser, index, s1 - s2);
	Encode_UTF8_Char(STR_SKIP(ser, index), codepoint);
}

REBU32 Decode_Surrogate_Pair(const REBYTE *src) {
	REBU32 c1 = ((src[0] & 0x0F) << 12) | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F);
	REBU32 c2 = ((src[3] & 0x0F) << 12) | ((src[4] & 0x3F) << 6) | (src[5] & 0x3F);
	if (c1 >= 0xD800 && c1 <= 0xDBFF && c2 >= 0xDC00 && c2 <= 0xDFFF) {
		return 0x10000 + ((c1 - 0xD800) << 10) + (c2 - 0xDC00);
	}
	return UNI_ERROR;
}

/***********************************************************************
**
*/	const REBYTE *UTF8_Check(const REBYTE *str, REBCNT len, REBFLG *surrogates)
/*
**		Returns 0 for success, else str where error occurred.
**
***********************************************************************/
{
	if (len == 0) return 0;
	const REBYTE *end = str + len;
	const REBYTE *acc = str - 1;
	REBCNT codepoint = 0;
	REBCNT state = UTF8_ACCEPT;
	*surrogates = FALSE;

	for (; str < end; ++str) {
		switch (UTF8_Decode_Step(&state, &codepoint, *str)) {
		case UTF8_ACCEPT: acc = str; break; // remember last accepted char position
		case UTF8_REJECT:
			if (Decode_Surrogate_Pair(str - 1) == UNI_ERROR) {
				return acc + 1;
			}
			*surrogates = TRUE;
			str += 4;
			acc = str;
			state = UTF8_ACCEPT;

			break;
		}
	}
	if (state == UTF8_ACCEPT) return 0;
	// if state is not accepted, we must have incomplete utf-8 sequence
	// not using str-1, because the sequence may have more than 2 bytes!
	return acc + 1;
}


/***********************************************************************
**
*/	REBSER *UTF8_Copy_Surrogates(const REBYTE *str, REBCNT len, REBCNT *err)
/*
**		Copy UTF-8 string while collapsing all surrogate pairs.
**
***********************************************************************/
{
	if (len == 0) return 0;
	const REBYTE *start = str;
	const REBYTE *end = str + len;
	const REBYTE *acc = start;
	REBCNT codepoint = 0;
	REBCNT state = UTF8_ACCEPT;

	REBSER *dst = Make_Series(len, 1, FALSE);

	for (; str < end; ++str) {
		switch (UTF8_Decode_Step(&state, &codepoint, *str)) {
		case UTF8_ACCEPT: acc = str + 1; break; // remember last accepted char position
		case UTF8_REJECT:
			codepoint = Decode_Surrogate_Pair(str - 1);
			if (codepoint != UNI_ERROR) {
				REBLEN bytes = str - 1 - start;
				if (bytes > 0) {
					Append_Bytes_Len(dst, cb_cast(start), bytes);
				}
				Append_Byte(dst, codepoint);
				str += 4;
				start = str;
				state = UTF8_ACCEPT;
			}
			break;
		}
	}
	if (state == UTF8_ACCEPT) {
		if (start < str) Append_Bytes_Len(dst, cb_cast(start), str - start);
		return dst;
	}
	else {
		// if state is not accepted, we must have incomplete utf-8 sequence or error!
		if (err) *err = (acc - start);
		return NULL;
	}
	return dst;
	
}

/***********************************************************************
**
*/	REBU32 UTF8_Decode_Codepoint(const REBYTE **RESTRICT str, REBCNT *RESTRICT len)
/*
**		Converts a single UTF8 code-point (to 32 bit).
**		Errors are returned as zero. (So prescan source for null.)
**		Increments str by extra chars needed.
**		Decrements len by extra chars needed.
**
***********************************************************************/
{
	REBYTE *src = (REBYTE *)*str;
	REBCNT codepoint = 0;
	REBCNT state = 0;
	REBCNT bytes = *len;

	for (; bytes > 0; ++src, --bytes) {
		// read bytes until codepoint is not complete or invalid...
		if (UTF8_Decode_Step(&state, &codepoint, *src)) {
			if (state != UTF8_REJECT) continue; // not yet complete
			// on reject, try to decode surrogate pair...
			if (bytes >= 5) {
				codepoint = Decode_Surrogate_Pair(src - 1);
				if (codepoint != UNI_ERROR) {
					src += 5;
					bytes -= 5;
					goto done;
				}
			}
		}
		++src; --bytes;
		break; // only one codepoint
	}
	if (state != UTF8_ACCEPT) codepoint = UNI_ERROR;
done:
	*len = bytes;
	*str = src;
	return codepoint;
}


/***********************************************************************
**
*/	REBSER* UTF8_To_UTF32(REBSER *dst_ser, const REBYTE *str, REBCNT len, REBFLG little_endian)
/*
***********************************************************************/
{
	REBLEN  dst_len = 0;
	REBYTE *dst_bin;
	REBU32 codepoint;

	const REBYTE *bp = str;
	REBYTE *cp;

	while (*bp) dst_len += (*bp++ & 0xC0) != 0x80;

	if (!dst_ser)
		dst_ser = Make_Series((dst_len + 1) * 4, 1, FALSE);
	else
		Expand_Series(dst_ser, 0, dst_len);

	dst_bin = BIN_HEAD(dst_ser);

	bp = str;
	while (*bp && len > 0) {
		codepoint = UTF8_Decode_Codepoint(&bp, &len);
		write_u32(dst_bin, codepoint, little_endian);
		dst_bin+=4;
	}
	SERIES_TAIL(dst_ser) = (dst_bin - BIN_HEAD(dst_ser));
	return dst_ser;
}


/***********************************************************************
**
*/	REBSER *UTF32_To_UTF8(REBSER *dst_ser, const REBYTE *str, REBCNT len, REBFLG little_endian)
/*
***********************************************************************/
{
	REBLEN  dst_len = 0, i;
	REBYTE *dst_bin;
	REBU32 codepoint;

	const REBU32 *uni = (REBU32*)str;
	const REBLEN uni_len = len / 4;
	REBYTE *cp;

	for (i = 0; i < uni_len; i++) {
		dst_len = UTF8_Codepoint_Size(uni[i]);
	}

	if (!dst_ser)
		dst_ser = Make_Series(dst_len+1, 1, FALSE);
	else
		Expand_Series(dst_ser, 0, dst_len);

	dst_bin = BIN_HEAD(dst_ser);


	for (i = 0; i < uni_len; i++) {
		dst_bin += Encode_UTF8_Char(dst_bin, uni[i]);
	}
	SERIES_TAIL(dst_ser) = (dst_bin - BIN_HEAD(dst_ser));
	return dst_ser;
}

// -------------------------------------------------------------------------


/***********************************************************************
************************************************************************
**
**	Code below added by REBOL Technologies 2008
**
************************************************************************
***********************************************************************/

/***********************************************************************
**
*/	REBINT What_UTF(const REBYTE *bp, REBCNT len)
/*
**		Tell us what UTF encoding the string has. Negative for LE.
**
***********************************************************************/
{
	// UTF8:
	if (len >= 3 && bp[0] == 0xef && bp[1] == 0xbb && bp[2] == 0xbf) return 8;

	if (len >= 2) {

		// UTF16:
		if (bp[0] == 0xfe && bp[1] == 0xff) return 16;

		// Either UTF16 or 32:
		if (bp[0] == 0xff && bp[1] == 0xfe) {
			if (len >= 4 && bp[2] == 0 && bp[3] == 0) return -32;
			return -16;
		}

		// UTF32
		if (len >= 4 && bp[0] == 0 && bp[1] == 0 && bp[2] == 0xfe && bp[3] == 0xff)
			return 32;
	}

	// Unknown:
	return 0;
}








/***********************************************************************
**
*/	int Decode_UTF8(REBUNI *dst, const REBYTE *src, REBCNT len, REBFLG ccr)
/*
**		Decode UTF8 byte string into a 16 bit preallocated array.
**
**		dst: the desination array, must always be large enough!
**		src: source binary data
**		len: byte-length of source (not number of chars)
**		ccr: convert CRLF/CR to LF
**
**		Returns length in chars (negative if all chars are latin-1).
**		No terminator is added.
**
***********************************************************************/
{
	int flag = -1;
	REBU32 ch;
	REBUNI *start = dst;

	while (len > 0) {
		if ((ch = *src) >= 0x80) {
			flag = 1;
			ch = UTF8_Decode_Codepoint(&src, &len);
			if (ch == 0) {
				ch = UNI_REPLACEMENT_CHAR; // temporary!
			}
			*dst++ = (REBUNI)ch;
			continue;	
		}
		len--;
		src++;
		if (ch == CR && ccr) {
			if (src[1] == LF) continue;
			ch = LF;
		}
		*dst++ = (REBUNI)ch;
	}

	return (dst - start) * flag;
}


/***********************************************************************
**
*/	REBSER *Decode_UTF_String(const REBYTE *bp, REBCNT len, REBINT utf, REBFLG ccr, REBCNT *err)
/*
**		Do all the details to decode a string.
**		Input is a byte series. Len is len of input.
**		The utf is 0, 8, +/-16, +/-32.
**		A special -1 means use the BOM.
**		Use `uni = TRUE` not to shorten ASCII result
**		ccr: convert CRLF 
* 
**
***********************************************************************/
{
	REBU32 codepoint;
	REBYTE *dst;
	REBCNT i = 0;
	REBCNT unit_size;
	REBFLG is_little_endian;

	if (len == 0) {
		return Make_Series(1, 1, FALSE);
	}

	if (utf == -1) {
		utf = What_UTF(bp, len);
		if (utf) {
			if (utf == 8) bp += 3, len -= 3;
			else if (utf == -16 || utf == 16) bp += 2, len -= 2;
			else if (utf == -32 || utf == 32) bp += 4, len -= 4;
		}
	}

	if (utf == 0 || utf == 8) {
		REBSER *ser = UTF8_Copy_Surrogates(bp, len, err);
		if (!ser) return NULL;
		if (ccr) {
			ser->tail = Replace_CRLF_to_LF_Bytes(BIN_HEAD(ser), BIN_LEN(ser));
		}
		if (!Is_ASCII(BIN_HEAD(ser), BIN_LEN(ser))) UTF8_SERIES(ser);
		return ser;
	} 
	else if (utf == -16 || utf == 16) {
		unit_size = 2;
	}
	else if (utf == -32 || utf == 32) {
		unit_size = 4;
	}
    else {
        return NULL; // Unknown UTF
    }

	is_little_endian = (utf < 0);
	dst = Reset_Buffer(BUF_SCAN, len); // should be large enough for the worst scenario
	const REBYTE *start = bp;
	const REBYTE *end = bp + len;
	while (bp < end) {
		// Read next code unit(s)
		if (unit_size == 2) {
			// UTF-16: handle surrogate pairs
			REBUNI w1 = read_u16(bp, is_little_endian);
			if (w1 >= 0xD800 && w1 <= 0xDBFF) {
				bp += 2;
				if (bp >= end) {
					// Truncated surrogate pair
					goto u16_error;
				}
				REBUNI w2 = read_u16(bp, is_little_endian);
				if (w2 < 0xDC00 || w2 > 0xDFFF) {
					// Invalid surrogate pair
					goto u16_error;
				}
				codepoint = 0x10000 + (((w1 - 0xD800) << 10) | (w2 - 0xDC00));
				bp += 2;
			}
			else if (w1 >= 0xDC00 && w1 <= 0xDFFF) {
				// Unpaired low surrogate
				goto u16_error;
			}
			else {
				codepoint = w1;
				bp += 2;
			}
		}
		else if (unit_size == 4) {
			// UTF-32: each unit is a codepoint
			codepoint = read_u32(bp, is_little_endian);
			// Validate data input
			if (codepoint > 0x10FFFF || (codepoint >= UNI_SUR_HIGH_START && codepoint <= UNI_SUR_LOW_END)) {
				if (err) *err = (bp - start);
				return NULL;
			}
			bp += 4;
		}
		dst += Encode_UTF8_Char(dst, codepoint);

		if (codepoint > 0x7F) UTF8_SERIES(BUF_SCAN);
	}
	len = dst - BIN_HEAD(BUF_SCAN);
	if (ccr) {
		len = Replace_CRLF_to_LF_Bytes(BIN_HEAD(BUF_SCAN), len);
	}
	return Copy_String(BUF_SCAN, 0, len);

u16_error:
	if (err) *err = 2 * (bp - 2 - start);
	return NULL;
}

/***********************************************************************
**
*/	REBCNT Length_As_UTF8_Code_Points(REBYTE *src)
/*
**		Returns number of code points encoded in UTF-8.
**
***********************************************************************/
{
	REBCNT size = 0;
	while (*src) {
        size += (*src++ & 0xC0) != 0x80;
    }
	return size;
}

/***********************************************************************
**
*/	REBCNT Length_As_UTF8(REBUNI *src, REBCNT len, REBOOL uni, REBOOL ccr)
/*
**		Returns how long the UTF8 encoded string would be.
**
***********************************************************************/
{


	REBCNT size = 0;
	REBCNT c;
	REBYTE *bp = (REBYTE*)src;

	for (; len > 0; len--) {
		c = uni ? *src++ : *bp++;
		if (c <= 0x7F) {
#ifdef TO_WINDOWS
			if (ccr && c == LF) size++; // because we will add a CR to it
#endif
			size++;
		}
		else if (c <= 0x7FF)        size += 2;
		else if (c <= 0xFFFF)       size += 3;
		else if (c <= UNI_MAX_LEGAL_UTF32) size += 4;
		else size += 3; // because of the replacement char size
	}

	return size;
}

FORCE_INLINE
/***********************************************************************
**
*/	REBCNT Encode_UTF8_Char(REBYTE *dst, REBU32 chr)
/*
**		Converts a single char to UTF8 code-point.
**		Returns length of char stored in dst.
**		Be sure dst has at least 4 bytes available.
**
***********************************************************************/
{
	if (chr <= 0x7F) {
		// 1-byte/7-bit ascii
		// (0b0xxxxxxx)
		dst[0] = (REBYTE)chr;
		return 1;
	}
	if (chr <= 0x7FF) {
		// 2-byte/11-bit utf8 code point
		// (0b110xxxxx 0b10xxxxxx)
		dst[0] = (REBYTE)(0xc0 | (REBYTE)((chr >> 6) & 0x1f));
		dst[1] = (REBYTE)(0x80 | (REBYTE)(chr & 0x3f));
		return 2;
	}
	if (chr <= 0xFFFF) {
		// 3-byte/16-bit utf8 code point
		// (0b1110xxxx 0b10xxxxxx 0b10xxxxxx)
		dst[0] = (REBYTE)(0xe0 | (REBYTE)((chr >> 12) & 0x0f));
		dst[1] = (REBYTE)(0x80 | (REBYTE)((chr >> 6) & 0x3f));
		dst[2] = (REBYTE)(0x80 | (REBYTE)(chr & 0x3f));
		return 3;
	}
	// 4-byte/21-bit utf8 code point
	// (0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx)
	dst[0] = (REBYTE)(0xf0 | (REBYTE)((chr >> 18) & 0x07));
	dst[1] = (REBYTE)(0x80 | (REBYTE)((chr >> 12) & 0x3f));
	dst[2] = (REBYTE)(0x80 | (REBYTE)((chr >> 6) & 0x3f));
	dst[3] = (REBYTE)(0x80 | (REBYTE)(chr & 0x3f));
	return 4;
}


/***********************************************************************
**
*/	REBCNT Encode_UTF8(REBYTE *dst, REBINT max, void *src, REBLEN *len, REBFLG uni, REBFLG ccr)
/*
**		Encode the unicode into UTF8 byte string.
**
**		Source string can be byte or unichar sized (uni = TRUE);
**		Max is the maximum size of the result (UTF8).
**		Returns number of source chars used.
**		Updates len for dst bytes used.
**		Does not add a terminator.
**
***********************************************************************/
{
	REBU32 c, c2;
	REBINT n;
	REBYTE buf[8];
	REBYTE *bs = dst; // save start
	REBYTE *bp;
	REBUNI *up;
	REBLEN cnt=0;

	if (len) cnt = *len;
	if (uni) {
		up = (REBUNI*)src;
		if (!cnt) {
			// not using wcslen, because on some systems wchar_t has 4 bytes!
			cnt = 0;
			while (*up++ != 0 && cnt < (REBLEN)max) cnt++;
			up = (REBUNI*)src;
		}
		for (; max > 0 && cnt > 0; cnt--) {
			c = *up++;
			if (c < 0x80) {
#if defined(TO_WINDOWS)
				if (ccr) {
					if (c == CR && up[0] == LF) {
						*dst++ = CR;
						*dst++ = LF;
						up++;
						cnt--;
						max -= 2;
						continue;
					}
					if (c == LF) {
						// If there's not room, don't try to output CRLF
						if (2 > max) { up--; break; }
						*dst++ = CR;
						max--;
						c = LF;
					}
				}
#endif
				*dst++ = (REBYTE)c;
				max--;
			}
			else {
				if (c >= 0xD800 && c <= 0xDBFF) {
					c2 = *up++; cnt--;
					c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00);
				}
				n = Encode_UTF8_Char(buf, c);
				if (n > max) { up--; break; }
				memcpy(dst, buf, n);
				dst += n;
				max -= n;
			}
		}
		if (len) *len = dst - bs;
		return up - (REBUNI*)src;
	}
	else {
		bp = (REBYTE*)src;
		if (!len) cnt = LEN_BYTES(bp);
		for (; max > 0 && cnt > 0; cnt--) {
			c = *bp++;
			if (c < 0x80) {
#if defined(TO_WINDOWS)
				if (ccr) {
					if (c == CR && bp[0] == LF) {
						*dst++ = CR;
						*dst++ = LF;
						bp++;
						cnt--;
						max -= 2;
						continue;
					}
					if (c == LF) {
						// If there's not room, don't try to output CRLF
						if (2 > max) { bp--; break; }
						*dst++ = CR;
						max--;
						c = LF;
					}
				}
#endif
				*dst++ = (REBYTE)c;
				max--;
			}
			else {
				n = Encode_UTF8_Char(buf, c);
				if (n > max) { bp--; break; }
				memcpy(dst, buf, n);
				dst += n;
				max -= n;
			}
		}
		if (len) *len = dst - bs;
		return bp - (REBYTE*)src;
	}


}

#ifdef unused
/***********************************************************************
**
X*/  int Encode_UTF8_Line(REBSER *dst, REBSER *src, REBCNT idx)
/*
**		Encode a unicode source buffer into a binary line of UTF8.
**		Include the LF terminator in the result.
**		Return the length of the line buffer.
**
***********************************************************************/
{
	REBUNI *up = UNI_HEAD(src);
	REBCNT len  = SERIES_TAIL(src);
	REBCNT tail;
	REBUNI c;
	REBINT n;
	REBYTE buf[8];

	tail = RESET_TAIL(dst);

	while (idx < len) {
		if ((c = up[idx]) < 0x80) {
			EXPAND_SERIES_TAIL(dst, 1);
			BIN_HEAD(dst)[tail++] = (REBYTE)c;
		}
		else {
			n = Encode_UTF8_Char(buf, c);
			EXPAND_SERIES_TAIL(dst, n);
			memcpy(BIN_SKIP(dst, tail), buf, n);
			tail += n;
		}
		idx++;
		if (c == LF) break;
	}

	BIN_HEAD(dst)[tail] = 0;
	SERIES_TAIL(dst) = tail;
	return idx;
}

/***********************************************************************
**
X*/	REBSER *Encode_UTF8_Value(REBVAL *arg, REBCNT len, REBFLG opts)
/*
**		Do all the details to encode a string as UTF8.
**		No_copy means do not make a copy.
**		Result can be a shared buffer!
**
***********************************************************************/
{
	REBSER * ser;
	if (VAL_BYTE_SIZE(arg)) {
		ser = Encode_UTF8_String(VAL_BIN_DATA(arg), len, FALSE, opts);
	} else {
		ser = Encode_UTF8_String(VAL_UNI_DATA(arg), len, TRUE, opts);
	}
	return ser;
}
#endif

/***********************************************************************
**
*/	REBSER *Encode_UTF8_String(void *src, REBLEN len, REBFLG uni, REBFLG opts)
/*
**		Do all the details to encode a string as UTF8.
**		No_copy means do not make a copy.
**		Result can be a shared buffer!
**
***********************************************************************/
{
	REBSER *ser; // a shared buffer
	REBLEN size;
	REBYTE *cp;
//	REBFLG ccr = GET_FLAG(opts, ENC_OPT_CRLF);
	REBFLG no_copy = GET_FLAG(opts, ENC_OPT_NO_COPY); // using share buffer

	if (uni) {
		REBYTE *utf8 = NULL;
		// Uasing OS conversion, because the old Rebol UTF-8 encoder does not support surrogates yet!
		size = OS_WIDE_TO_MULTIBYTE((const REBCHR *)src, &utf8, len);
		if (no_copy) {
			ser = BUF_SCAN;
			cp = Reset_Buffer(ser, size); // +(GET_FLAG(opts, ENC_OPT_BOM) ? 3 : 0));
			COPY_MEM(cp, utf8, size);
			SERIES_TAIL(ser) = size;
			STR_TERM(ser);
		}
		else {
			ser = Copy_Bytes(utf8, size);
		}
		OS_FREE(utf8);
	}
	else {
		size = len;
		ser = Copy_Bytes((REBYTE *)src, size);
#ifdef unused
		if (ccr || !Is_ASCII(bp, len)) {
			size = Length_As_UTF8((REBUNI*)bp, len, FALSE, (REBOOL)ccr);
			cp = Reset_Buffer(ser, size + (GET_FLAG(opts, ENC_OPT_BOM) ? 3 : 0));
			Encode_UTF8(cp, size, bp, &len, FALSE, ccr);
		}
		else if (GET_FLAG(opts, ENC_OPT_NO_COPY)) return 0;
		else return Copy_Bytes(bp, len);
#endif
	}
	if (!Is_ASCII(BIN_HEAD(ser), size)) UTF8_SERIES(ser);
	return ser;
}
