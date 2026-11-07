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
**  Module:  u-bincode.c
**  Summary: binary data encoder/decoder
**  Section: utility
**  Author:  Oldes
**  Version: 1.0.0
**  History: 15-10-2018 Initial release with limited set of functionality
**           07-11-2026 Context is a `bincode` handle; read/write symmetry; many fixes
**
***********************************************************************/
/*
	Usage:

	bin: binary 256 ;creates ctx of size 256 bytes
	;or
	bin: binary #{7B1948656C6C6F00} ;creates ctx and inits buffers from given binary
	;which is same like:
	bin: binary none
	bin: binary/write bin #{7B1948656C6C6F00}

	binary/read bin 'length?  ; <- number of available bytes in read-buffer
	;== 8 

	binary/read bin 'bytes ; <- read all remaining bytes
	;== #{7B1948656C6C6F00} 

	binary/init bin 1024  ; resets existing ctx and makes sure it has space for at least 1024 bytes
	binary/init bin #{FF} ; resets existing ctx and fills it with the given binary

	binary/read b 'ui8    ; to read just single byte as integer

	binary/init bin none  ; clears buffers, preallocated space is still available

	;; NOTE: not implemented, just an idea...
	;- using port?
	bin: open binary://
	write bin [UI16 6523 STRING "Hello"]
	;which would create internal binary: #{7B1948656C6C6F00}

	read bin ;would return #{7B1948656C6C6F00}
	;or
	read/as bin [UI16 STRING] ;would return [6523 "Hello"]
	;or
	read/as bin [size: UI16 name: STRING] ;could return [size: 6523 name: "Hello"]
	close bin
*/

#ifndef IGNORE_BINCODE

#include "sys-core.h"
#include "reb-ext.h" // RXT_* types used by the handle path callbacks
#include <stdio.h>
#include <time.h>

// FLOAT16 credits: Steven Pigeon
// https://hbfs.wordpress.com/2013/02/12/float16/
typedef union {
	struct {
		// type determines alignment!
		u16 m : 10;
		u16 e : 5;
		u16 s : 1;
	} bits;
	struct {
		u8 low;
		u8 high;
	} bytes;
} float16_s;

typedef union {
	float v;
	struct {
		u32 m : 23;
		u32 e : 8;
		u32 s : 1;
	} bits;
} float32_s;

float float16to32(float16_s f16) {
	// back to 32
	float32_s f32;
#ifndef USE_NO_INFINITY 
	if (f16.bits.e == 31) { // inifinity or nan
		return (f16.bits.m == 0) ? (f16.bits.s ? -1.0 : 1.0) * INFINITY : NAN;
	}
#endif
	if (f16.bits.e == 0) {
		// zero or subnormal: value = m * 2^-24
		f32.v = (float)f16.bits.m / 16777216.0f;
		return f16.bits.s ? -f32.v : f32.v;
	}
	f32.bits.s = f16.bits.s;
	f32.bits.e = (f16.bits.e - 15) + 127; // safe in this direction
	f32.bits.m = ((u32)f16.bits.m) << 13;
	return f32.v;
}

// Converts a double to IEEE 754 half precision bits, rounding to nearest even
// (directly from the double, so there is no double rounding through a float).
// Overflow gives infinity, small values are stored as subnormals.
static u16 Double_To_Half(double d) {
	u64 b, man, h, rem, halfway;
	u16 sign;
	int exp, e, shift;

	memcpy(&b, &d, 8);
	sign = (u16)((b >> 48) & 0x8000);
	exp  = (int)((b >> 52) & 0x7FF);
	man  = b & 0xFFFFFFFFFFFFFULL;

	if (exp == 0x7FF) return man ? 0x7E00 : (sign | 0x7C00); // canonical NaN (without sign) or infinity
	if (exp == 0) return sign;                // zero or double subnormal (too small)
	e = exp - 1023 + 15;                      // half's biased exponent
	if (e >= 31) return sign | 0x7C00;        // overflow
	if (e < -10) return sign;                 // below half of the smallest subnormal
	man |= 1ULL << 52;                        // implicit bit
	if (e > 0) { shift = 42; h = ((u64)e << 10) | ((man >> 42) & 0x3FF); } // normal
	else       { shift = 43 - e; h = man >> shift; }                       // subnormal
	rem = man & ((1ULL << shift) - 1);
	halfway = 1ULL << (shift - 1);
	// a carry from the mantissa correctly moves to the exponent (also to infinity)
	if (rem > halfway || (rem == halfway && (h & 1))) h++;
	return sign | (u16)h;
}

// MS-DOS date and time are little endian 16-bit values:
//   time: bits 0-4 seconds/2, 5-10 minutes, 11-15 hours
//   date: bits 0-4 day, 5-8 month, 9-15 years since 1980
//   datetime: time followed by date (4 bytes)
// Explicit shifts are used instead of bitfields, whose layout is implementation
// defined and which would access 4 bytes when only 2 are available.
static u16 Msdos_Date_Bits(REBVAL *v) {
	return (u16)((((VAL_YEAR(v) - 1980) & 0x7F) << 9) | ((VAL_MONTH(v) & 0x0F) << 5) | (VAL_DAY(v) & 0x1F));
}
static u16 Msdos_Time_Bits(REBVAL *v) {
	i64 secs, h, m;
	if (VAL_TIME(v) == NO_TIME) return 0;
	secs = VAL_TIME(v) / SEC_SEC;
	h = secs / 3600; secs -= h * 3600;
	m = secs / 60;   secs -= m * 60;
	return (u16)(((h & 0x1F) << 11) | ((m & 0x3F) << 5) | ((secs / 2) & 0x1F)); // only 2 sec resolution!
}
#define MSDOS_GET_U16(p)    ((u16)((p)[0] | ((p)[1] << 8)))
#define MSDOS_PUT_U16(p, n) ((p)[0] = (REBYTE)(n), (p)[1] = (REBYTE)((n) >> 8))
#define MSDOS_TIME_SECS(t)  ((((t) >> 11) & 0x1F) * 3600 + (((t) >> 5) & 0x3F) * 60 + ((t) & 0x1F) * 2)

#define ASSERT_SI_RANGE(v, n) if (Bincode_Int_Arg(v) < (- (i64)(n) - 1) || Bincode_Int_Arg(v) > (i64)(n)) Trap1(RE_OUT_OF_RANGE, v); // -(n+1) .. n
// integer! or char! argument as a 64-bit value (char! uses only 32 bits of the value data)
static REBI64 Bincode_Int_Arg(REBVAL *v) {
	return IS_CHAR(v) ? (REBI64)VAL_CHAR(v) : VAL_INT64(v);
}
#define ASSERT_UI_RANGE(v, n) if (Bincode_Int_Arg(v) < -(REBI64)(n) || Bincode_Int_Arg(v) > (REBI64)(n)) Trap1(RE_OUT_OF_RANGE, v); // negative allowed (two's complement), like ASSERT_U32_RANGE
#define ASSERT_UIBYTES_RANGE(v, n) if (VAL_LEN(v) > n) Trap1(RE_OUT_OF_RANGE, v);
#define ASSERT_U32_RANGE(v) if (Bincode_Int_Arg(v) > (REBI64)0xFFFFFFFFLL || Bincode_Int_Arg(v) < -(REBI64)0xFFFFFFFFLL) Trap1(RE_OUT_OF_RANGE, v);
#define ASSERT_READ_SIZE(v, p, e, n) if((p + n) > e) Trap1(RE_OUT_OF_RANGE, v);
#define ASSERT_INDEX_RANGE(v, p, e) if(i < 0 || i > VAL_TAIL(v)) Trap1(RE_OUT_OF_RANGE, e);

// Resolves a :get-word or :get-path argument (a path result is a volatile stack reference!)
#define BINCODE_GET_ARG(v) \
	do { \
		if (IS_GET_WORD(v)) v = Get_Var(v); \
		else if (IS_GET_PATH(v)) { Do_Path(&v, NULL); v = DS_POP; } \
	} while (0)
#define IS_BIT_SET(v, b) ((v & b) == b)
#define NEXT_IN_BIT(inBit) \
	do { \
		inBit = inBit >> 1; \
		if (inBit == 0) { \
			inBit = 0x80; \
			ASSERT_READ_SIZE(value, cp, ep, 1); \
			cp++; \
			VAL_INDEX(buffer_read)++; \
		} \
	} while (0)
#define STORE_IN_BIT(val, inBit)  (r_mask = (inBit)) // committed into the context on success

//**********************************************************************
// Bincode context is a BINCODE handle. Its buffer is kept in hob->series
// (marked by GC), the positions and bit masks in this struct.
typedef struct rebol_bincode_ctx {
	REBCNT r_index;  // read position
	REBCNT w_index;  // write position
	REBCNT r_mask;   // read bit mask (0 = no bit read yet in the current byte)
	REBCNT w_mask;   // write bit mask (0 = no partially written byte)
	REBFLG owned;    // TRUE when the buffer was made by bincode (not user's binary)
} BINCODE_CTX;

// Commits the local buffer positions and masks into the context
#define BINCODE_COMMIT() (bc->r_index = VAL_INDEX(buffer_read), bc->w_index = VAL_INDEX(buffer_write), \
                          bc->r_mask = r_mask, bc->w_mask = w_mask)
#define BINCODE_RETURN(r) do { BINCODE_COMMIT(); return (r); } while (0)


static REBCNT EncodedU64_Size(u64 value) {
	REBCNT count = 1;
	while ((value >>= 7) != 0) count++;
	return count;
}

static REBCNT EncodedVINT_Size(REBU64 value) {
	REBCNT count = 1;
	// all-ones value of a width is reserved (EBML "unknown size"), so it moves to the next width
	while (value >= (1ULL << (7 * count)) - 1) count++;
	return count;
}

#define IS_BINCODE_BIT_CMD(c) ((c) == SYM_BIT || (c) == SYM_NOT_BIT || (c) == SYM_UB || (c) == SYM_SB || (c) == SYM_FB)

// Validates UB/SB/FB write value for given number of bits (0-64)
// and returns its bit pattern (lowest nbits bits are used).
static u64 Bincode_Bits_Value(REBCNT cmd, REBINT nbits, REBVAL *arg) {
	i64 v, lo, hi;
	if (cmd == SYM_FB) {
		REBDEC d = (IS_INTEGER(arg) ? (REBDEC)VAL_INT64(arg) : VAL_DECIMAL(arg)) * 65536.0;
		if (!(d >= -9223372036854775808.0 && d < 9223372036854775808.0)) Trap1(RE_OUT_OF_RANGE, arg);
		v = (i64)(d < 0 ? d - 0.5 : d + 0.5);
	} else {
		v = VAL_INT64(arg);
	}
	if (nbits == 0) {
		if (v != 0) Trap1(RE_OUT_OF_RANGE, arg);
		return 0;
	}
	if (nbits < 64) {
		// UB accepts signed or unsigned interpretation, SB/FB only signed
		lo = -((i64)1 << (nbits - 1));
		hi = (cmd == SYM_UB) ? (i64)(((u64)1 << nbits) - 1) : ((i64)1 << (nbits - 1)) - 1;
		if (v < lo || v > hi) Trap1(RE_OUT_OF_RANGE, arg);
		return (u64)v & (((u64)1 << nbits) - 1);
	}
	return (u64)v;
}

// Should vector data be byte swapped for the given command (VECTOR, VECTOR-BE or VECTOR-LE)?
static REBOOL Bincode_Swap_Needed(REBCNT cmd) {
	const u16 one = 1;
	REBOOL host_le = (*(const u8*)&one == 1);
	return (cmd == SYM_VECTOR_BE && host_le) || (cmd == SYM_VECTOR_LE && !host_le);
}
// vector element widths are 1, 2, 4 or 8 (struct elements are never swapped)
static void Bincode_Swap_Endian(REBYTE *p, REBCNT len, REBCNT wide) { if (wide > 1) Swap_Endian(p, len, wide); }
#define VECTOR_DATA_AT(v) SERIES_SKIP(VAL_SERIES(v), VAL_INDEX(v))
#define VECTOR_BYTES(v)   ((REBU64)VAL_LEN(v) * SERIES_WIDE(VAL_SERIES(v)))

// Number of bytes of a DER length (definite form, minimal encoding, max 4 length bytes)
static REBCNT Der_Length_Size(u64 v) {
	REBCNT n = 1;
	if (v < 128) return 1; // short form
	while (v > 0xFF) { v >>= 8; n++; }
	return n + 1;         // long form: 0x80|n followed by n bytes
}
// Number of octal digits needed for a value
static REBCNT Octal_Digits(u64 v) {
	REBCNT d = 1;
	while (v >>= 3) d++;
	return d;
}
#define BITSET_CMD_BYTES(c) ((c) == SYM_BITSET8 ? 1 : (c) == SYM_BITSET16 ? 2 : 4)
// Validates that a bitset fits into `n` bytes (no bits set above them, not negated)
static void Bincode_Check_Bitset(REBVAL *v, REBCNT n) {
	REBSER *ser = VAL_SERIES(v);
	REBCNT i;
	if (VAL_BITSET_NOT(v)) Trap1(RE_INVALID_ARG, v);
	for (i = n; i < SERIES_TAIL(ser); i++) {
		if (BIN_DATA(ser)[i]) Trap1(RE_OUT_OF_RANGE, v);
	}
}
// Converts a FIXED8 (signed 8.8) or FIXED16 (signed 16.16) write argument
// to its rounded integer representation (traps when out of range)
static i64 Bincode_Fixed_Value(REBCNT cmd, REBVAL *arg) {
	REBDEC d = IS_INTEGER(arg) ? (REBDEC)VAL_INT64(arg) : VAL_DECIMAL(arg);
	REBDEC lo = (cmd == SYM_FIXED8) ? -32768.0 : -2147483648.0;
	REBDEC hi = (cmd == SYM_FIXED8) ?  32767.0 :  2147483647.0;
	d *= (cmd == SYM_FIXED8) ? 256.0 : 65536.0;
	d = (d < 0) ? d - 0.5 : d + 0.5; // round half away from zero (truncated below)
	if (!(d > lo - 1.0 && d < hi + 1.0)) Trap1(RE_OUT_OF_RANGE, arg); // also NaN
	return (i64)d;
}

/***********************************************************************
**
*/	static int Bincode_Get_Path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg)
/*
**		ctx/buffer and ctx/buffer-write are the buffer at the read/write
**		position, ctx/r-mask and ctx/w-mask the bit masks.
**
***********************************************************************/
{
	BINCODE_CTX *bc = (BINCODE_CTX*)hob->data;
	switch (word) {
	case SYM_BUFFER:
	case SYM_BUFFER_WRITE:
		arg->series = hob->series;
		arg->index  = (word == SYM_BUFFER) ? bc->r_index : bc->w_index;
		if (arg->index > SERIES_TAIL(hob->series)) arg->index = SERIES_TAIL(hob->series);
		*type = RXT_BINARY;
		break;
	case SYM_R_MASK:
		*type = RXT_INTEGER;
		arg->int64 = bc->r_mask;
		break;
	case SYM_W_MASK:
		*type = RXT_INTEGER;
		arg->int64 = bc->w_mask;
		break;
	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}

/***********************************************************************
**
*/	static int Bincode_Set_Path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg)
/*
**		Allows setting positions in the context's own buffer and the bit masks.
**
***********************************************************************/
{
	BINCODE_CTX *bc = (BINCODE_CTX*)hob->data;
	switch (word) {
	case SYM_BUFFER:
	case SYM_BUFFER_WRITE:
		if (*type != RXT_BINARY) return PE_BAD_SET_TYPE;
		// only a position in the context's own buffer (use /init to use other data)
		if (arg->series != hob->series || arg->index > SERIES_TAIL(hob->series)) return PE_BAD_SET;
		if (word == SYM_BUFFER) bc->r_index = arg->index;
		else bc->w_index = arg->index;
		break;
	case SYM_R_MASK:
	case SYM_W_MASK:
		if (*type != RXT_INTEGER) return PE_BAD_SET_TYPE;
		// 0 or a single bit (a partially written byte has at least one bit written)
		if (arg->int64 < 0 || arg->int64 > ((word == SYM_R_MASK) ? 0x80 : 0x40)
			|| (arg->int64 & (arg->int64 - 1))) return PE_BAD_RANGE;
		if (word == SYM_R_MASK) bc->r_mask = (REBCNT)arg->int64;
		else bc->w_mask = (REBCNT)arg->int64;
		break;
	default:
		return PE_BAD_SET;
	}
	return PE_OK;
}

/***********************************************************************
**
*/	static int Bincode_Mold(REBHOB *hob, REBSER *str)
/*
**		Molds the handle's info, like: 0#55d0c8a1b2e0 r: 3 w: 5 len: 16
**		The pointer identifies the context in logs (stable for its lifetime).
**		Bit masks are included only when not zero.
**
***********************************************************************/
{
	BINCODE_CTX *bc;
	char buf[128];

	if (!str || !hob || !(bc = (BINCODE_CTX*)hob->data)) return 0;

	SERIES_TAIL(str) = 0;
	// unsigned long long, because long is only 32-bit on 64-bit Windows
	snprintf(buf, sizeof(buf), "0#%llx r: %u w: %u len: %u",
		(unsigned long long)(REBUPT)hob->data, bc->r_index, bc->w_index,
		hob->series ? SERIES_TAIL(hob->series) : 0);
	Append_Bytes(str, buf);
	if (bc->r_mask || bc->w_mask) {
		snprintf(buf, sizeof(buf), " r-mask: %u w-mask: %u", bc->r_mask, bc->w_mask);
		Append_Bytes(str, buf);
	}
	return (int)SERIES_TAIL(str);
}

/***********************************************************************
**
*/	void Init_Bincode(void)
/*
**		Registers the BINCODE handle type.
**
***********************************************************************/
{
	REBHSP spec;
	CLEARS(&spec);
	spec.size     = sizeof(BINCODE_CTX);
	spec.get_path = Bincode_Get_Path;
	spec.set_path = Bincode_Set_Path;
	spec.mold     = Bincode_Mold;
	Register_Handle_Spec(SYM_BINCODE, &spec);
}

/***********************************************************************
**
*/	REBNATIVE(binary)
/*
//	binary: native [
//		"Entry point of the binary DSL (Bincode)"
//
//		ctx [handle! binary! integer! none!] "Bincode context. If none, it will create a new one."
//		/init    "Initializes buffers in the context"
//			spec [binary! integer! none!]
//		/write   "Write data into output buffer"
//			data [binary! block!] "Data dialect"
//		/read    "Read data from the input buffer"
//			code [word! block! integer! binary! struct!]   "Input encoding"
//		/into    "Put READ results in out block, instead of creating a new block"
//			out  [block! struct! vector!] "Target block for results, or a struct/vector to be filled, when /into is used"
//		/with    "Additional input argument"
//			num  [integer!] "Bits/bytes number used with WORD! code type to resolve just single value"
//	]
***********************************************************************/
{
	REBVAL *val_ctx   = D_ARG(1); 
	REBOOL  ref_init  = D_REF(2);
	REBVAL *val_spec  = D_ARG(3);
	REBOOL  ref_write = D_REF(4);
	REBVAL *val_write = D_ARG(5);
	REBOOL  ref_read  = D_REF(6);
	REBVAL *val_read  = D_ARG(7);
	REBOOL  ref_into  = D_REF(8);
	REBVAL *val_into  = D_ARG(9);
//	REBOOL  ref_with  = D_REF(10);
	REBVAL *val_num   = D_ARG(11);

	REBVAL *ret = D_RET;
	//REBVAL *buf;
	REBSER *bin, *bin_new;
	REBHOB *hob;
	BINCODE_CTX *bc;
	REBVAL buf_w, buf_r; // local views of the context's buffer (committed on success)
	REBVAL *buffer_write = &buf_w;
	REBVAL *buffer_read  = &buf_r;
	REBCNT r_mask, w_mask;
	REBFLG existing = FALSE;
	REBCNT inBit, nbits;
	REBYTE *cp, *bp, *ep;
	REBCNT n, count, index, tail, tail_new;
	REBDEC dbl;
	REBCNT cmd;
	REBLEN i, len;
	REBINT j;
	u64 u;
	i64 si;
	u32 ulong;
	u16 ushort;
	float16_s f16;
	float32_s f32;
	REBYTE buf[8];


	u16 msdos_bits;

	REBVAL *value, *next;
	REBVAL *data;
	REBSER *blk = NULL; //used to store results of the read action
	REBVAL *temp;

	//printf("\nDS_TOP start: %x\n", DS_TOP);

	if (IS_BINARY(val_ctx) || IS_NONE(val_ctx) || IS_INTEGER(val_ctx)) {
		// make a new context
		REBVAL hnd;
		REBCNT start = 0;
		REBFLG owned = !IS_BINARY(val_ctx);
		if (IS_BINARY(val_ctx)) {
			bin = VAL_SERIES(val_ctx);
			start = VAL_INDEX(val_ctx);
			n = 0;
		} else {
			bin = NULL;
			if (IS_INTEGER(val_ctx) && (VAL_INT64(val_ctx) < 0 || VAL_INT64(val_ctx) > 0x7FFFFFFF)) Trap_Arg(val_ctx);
			n = (IS_INTEGER(val_ctx)) ? VAL_INT32(val_ctx) : 1024;
			if (n <= 1) n = 16;
		}
		VAL_HANDLE_FLAGS(&hnd) = 0; // SET_HANDLE ORs into this
		MAKE_HANDLE(&hnd, SYM_BINCODE);
		hob = VAL_HANDLE_CTX(&hnd);
		if (!hob) Trap0(RE_NO_MEMORY);
		bc = (BINCODE_CTX*)hob->data;
		CLEARS(bc);
		hob->series = bin;
		*val_ctx = hnd; // the handle (and so the buffer) is referenced from now
		if (!bin) bin = hob->series = Make_Binary(n - 1);
		bc->r_index = bc->w_index = start;
		bc->owned = owned;
	}
	else if (IS_HANDLE(val_ctx) && IS_CONTEXT_HANDLE(val_ctx) && IS_VALID_CONTEXT_HANDLE(val_ctx, SYM_BINCODE)) {
		hob = VAL_HANDLE_CTX(val_ctx);
		bc = (BINCODE_CTX*)hob->data;
		bin = hob->series;
		existing = TRUE;
	}
	else Trap_Arg(val_ctx);

	// The buffer may be modified by user code between calls (it is accessible as ctx/buffer)
	if (bc->r_index > SERIES_TAIL(bin)) bc->r_index = SERIES_TAIL(bin);
	if (bc->w_index > SERIES_TAIL(bin)) bc->w_index = SERIES_TAIL(bin);

	// Local views of the context; the context is updated only when the call succeeds
	SET_BINARY(buffer_read,  bin);
	SET_BINARY(buffer_write, bin);
	VAL_INDEX(buffer_read)  = bc->r_index;
	VAL_INDEX(buffer_write) = bc->w_index;
	r_mask = bc->r_mask;
	w_mask = bc->w_mask;

	if (ref_init && existing) {
		// reset the existing context
		VAL_INDEX(buffer_write) = VAL_INDEX(buffer_read) = 0;
		r_mask = w_mask = 0;
		switch(VAL_TYPE(val_spec)) {
			case REB_BINARY:
				n = VAL_TAIL(val_spec) - VAL_INDEX(val_spec);
				//Dump_Series(bin, "BIN1:");
				if (n >= SERIES_REST(bin)) { // keep space for the terminator
					Expand_Series(bin, AT_TAIL, n - SERIES_TAIL(bin));
				}
				// the spec may be a part of the context's own buffer, so the data may overlap!
				memmove(BIN_DATA(bin), VAL_DATA(val_spec), n);
				VAL_TAIL(buffer_write) = VAL_TAIL(buffer_read) = n;
				//Dump_Series(bin, "BIN2:");
				break;
			case REB_INTEGER:
				//printf("resize from %i, needs %i\n", SERIES_REST(bin), VAL_INT32(val_spec));
				if (VAL_INT64(val_spec) < 0 || VAL_UNT64(val_spec) >= MAX_U32)
					Trap1(RE_INVALID_ARG, val_spec);
				if (VAL_UNT32(val_spec) >= SERIES_REST(bin)) { // keep space for the terminator
					Expand_Series(bin, AT_TAIL, VAL_INT32(val_spec) - SERIES_TAIL(bin));
				}
				//printf("resiz? new: %i\n", SERIES_REST(bin));
				VAL_TAIL(buffer_write) = VAL_TAIL(buffer_read) = 0;
				//Dump_Series(bin, "BIN2:");
				break;
			case REB_NONE:
				// just reset the index and tail pointers
				VAL_INDEX(buffer_write) = VAL_INDEX(buffer_read) = 0;
				VAL_TAIL(buffer_write) = VAL_TAIL(buffer_read) = 0;
				break;
		}
	}

	if (ref_write && IS_PROTECT_SERIES(bin)) Trap0(RE_PROTECTED);

	//printf("bincode buffer rest: %i tail: %i index: %i\n", SERIES_REST(bin), bin->tail, VAL_INDEX(buffer_write));

	REBOOL as_block;
	REBVAL char_arg; // char! argument converted to integer! (only 32 bits of a char's value data are valid)
	REBCNT outBit = 0, pb = 0; // write bit mask / bits already written in current byte

	if(ref_write) {
		//printf("In at: %i\n", VAL_INDEX(in));

		//puts("-----------write");
		//Dump_Series(bin, "BIN");

		DS_PUSH_NONE;
		temp = DS_TOP;

		index = VAL_INDEX(buffer_write);
		tail  = VAL_TAIL(buffer_write);

		switch(VAL_TYPE(val_write)) {
		case REB_BINARY:
			// optimized version for RAW binary input
			len = VAL_LEN(val_write);

			if (w_mask) {
				// finish partially written byte
				VAL_INDEX(buffer_write)++;
				index++;
				w_mask = 0;
			}
			tail_new = index + len; //bytes needed to write
			//printf("index: %i tail: %i len: %i tail_new: %i rest: %i\n", index, tail, len, tail_new, SERIES_REST(bin));
			if (tail_new >= SERIES_REST(bin)) { // keep space for the terminator
				//printf("expanding by delta: %i\n", tail_new - SERIES_TAIL(bin));
				Expand_Series(bin, AT_TAIL, tail_new - SERIES_TAIL(bin));
				//Dump_Series(bin, "BIN");
			}
			if(tail_new > tail) {
				SERIES_TAIL(bin) = tail_new;
				VAL_TAIL(buffer_read) = tail_new;
			}
			memcpy(VAL_BIN_AT(buffer_write), VAL_BIN_AT(val_write), len);
			VAL_INDEX(buffer_write) += len;
			//Dump_Series(bin, "NewBin=>");
			break;

		case REB_BLOCK:
			// writing data specified in a WRITE dialect
			value = VAL_BLK_DATA(val_write);
			if (IS_END(value)) break; //empty block (DS_DROP below; /read may follow)

			// try to predict number of bytes needed...
			count = VAL_INDEX(buffer_write);
			outBit = w_mask & 0xFF;
			for (pb = 0, nbits = outBit; nbits && nbits < 0x80; nbits <<= 1) pb++;

			for (; NOT_END(value); value++) {
				switch (VAL_TYPE(value)) {
				case REB_GET_WORD:
					data = Get_Var(value);
					break;
				//case REB_GET_PATH:
				//	data = Do_Path(&value, NULL);
				//	data = DS_POP; // volatile stack reference
				//	break;
				default:
					data = value;
				}
				switch (VAL_TYPE(data)) {
				case REB_BINARY:
				case REB_STRING:
				case REB_FILE:
				case REB_URL:
				case REB_EMAIL:
					if (pb) { count++; pb = 0; } // align
					count += VAL_LEN(data);
					break;
				case REB_TUPLE:
					// raw tuple bytes (read using TUPLE3 or TUPLE4)
					if (pb) { count++; pb = 0; } // align
					count += VAL_TUPLE_LEN(data);
					break;
				case REB_VECTOR:
					// raw vector data (native byte order)
					if (pb) { count++; pb = 0; } // align
					if (VECTOR_BYTES(data) > 0x7FFFFFFF) Trap1(RE_SIZE_LIMIT, data);
					count += (REBCNT)VECTOR_BYTES(data);
					break;
				case REB_STRUCT:
					// raw struct data (structs holding Rebol values would write pointers)
					if (VAL_STRUCT_PROTECTED(data)) Trap0(RE_PROTECTED);
					if (pb) { count++; pb = 0; } // align
					count += VAL_STRUCT_SIZE(data);
					break;
				case REB_WORD:
					next = ++value;
					if (IS_GET_WORD(next)) {
						next = Get_Var(next);
					} else if (IS_GET_PATH(next)) {	
						Do_Path(&next, NULL);
						next = DS_POP; // volatile stack reference
					}
					cmd = VAL_WORD_CANON(data);
					if (pb && !IS_BINCODE_BIT_CMD(cmd)) { count++; pb = 0; } // byte commands start at byte boundary
					switch (cmd) {
					case SYM_SI8:
					case SYM_UI8:
						if (IS_INTEGER(next) || IS_CHAR(next)) {
							// range check before the buffer is modified
							if (cmd == SYM_SI8) { ASSERT_SI_RANGE(next, 0x7F) }
							else { ASSERT_UI_RANGE(next, 0xFF) }
							count += 1;
						}
						else goto error;
						break;
					case SYM_SI16:
					case SYM_SI16BE:
					case SYM_SI16LE:
					case SYM_UI16:
					case SYM_UI16LE:
					case SYM_UI16BE:
						if (IS_INTEGER(next) || IS_CHAR(next)) {
							// range check before the buffer is modified
							if (cmd == SYM_SI16 || cmd == SYM_SI16BE || cmd == SYM_SI16LE) { ASSERT_SI_RANGE(next, 0x7FFF) }
							else { ASSERT_UI_RANGE(next, 0xFFFF) }
							count += 2;
						}
						else goto error;
						break;
					case SYM_SI24:
					case SYM_SI24BE:
					case SYM_SI24LE:
					case SYM_UI24:
					case SYM_UI24LE:
					case SYM_UI24BE:
						if (IS_INTEGER(next) || IS_CHAR(next)) {
							// range check before the buffer is modified
							if (cmd == SYM_SI24 || cmd == SYM_SI24BE || cmd == SYM_SI24LE) { ASSERT_SI_RANGE(next, 0x7FFFFF) }
							else { ASSERT_UI_RANGE(next, 0xFFFFFF) }
							count += 3;
						}
						else goto error;
						break;
					case SYM_SI32:
					case SYM_SI32BE:
					case SYM_SI32LE:
					case SYM_UI32:
					case SYM_UI32LE:
					case SYM_UI32BE:
						if (IS_INTEGER(next) || IS_CHAR(next)) {
							// range check before the buffer is modified
							if (cmd == SYM_SI32 || cmd == SYM_SI32BE || cmd == SYM_SI32LE) { ASSERT_SI_RANGE(next, 0x7FFFFFFF) }
							else { ASSERT_U32_RANGE(next) }
							count += 4;
						}
						else goto error;
						break;
					case SYM_SI64:
					case SYM_SI64BE:
					case SYM_SI64LE:
					case SYM_UI64:
					case SYM_UI64LE:
					case SYM_UI64BE:
						if (IS_INTEGER(next) || IS_CHAR(next)) {
							count += 8;
							continue;
						}
						goto error;
					case SYM_AT:
					case SYM_ATZ:
						if (IS_INTEGER(next)) {
							if (count > tail) tail = count;
							j = (cmd == SYM_AT ? 1 : 0);
							// AT is using ABSOLUTE positioning, so it cannot be < 1 (one-indexed) or < 0 (zero-based)
							if(VAL_INT32(next) < j) Trap1(RE_OUT_OF_RANGE, next);
							count = VAL_INT32(next) - j;
							continue;
						}
						goto error;
					case SYM_SKIP:
						// relative to the write position (may be negative, may go past the tail)
						if (!IS_INTEGER(next)) goto error;
						if (count > tail) tail = count;
						si = (REBI64)count + VAL_INT64(next);
						if (si < 0 || si > 0x7FFFFFFF) Trap1(RE_OUT_OF_RANGE, next);
						count = (REBCNT)si;
						continue;
					case SYM_PAD:
						if (IS_INTEGER(next)) {
							if (VAL_INT32(next) <= 0) Trap1(RE_INVALID_ARG, next);
							i = count % VAL_INT32(next);
							count += (i > 0) ? VAL_UNT32(next) - i : 0;
							continue;
						}
						goto error;
					case SYM_STRING:
						if (ANY_BINSTR(next)) {
							count += VAL_LEN(next) + 1; // + NUL terminator
							continue;
						}
						goto error;
					case SYM_BYTES:
						if (ANY_BINSTR(next)) {
							count += VAL_LEN(next);
							continue;
						}
						goto error;
					case SYM_UI8BYTES:
						if (ANY_BINSTR(next)) {
							ASSERT_UIBYTES_RANGE(next, 0xFF);
							count += (1 + VAL_LEN(next));
						}
						else goto error;
						break;
					case SYM_UI16BYTES:
					case SYM_UI16LEBYTES:
					case SYM_UI16BEBYTES:
						if (ANY_BINSTR(next)) {
							ASSERT_UIBYTES_RANGE(next, 0xFFFF);
							count += (2 + VAL_LEN(next));
							continue;
						}
						goto error;
					case SYM_UI24BYTES:
					case SYM_UI24LEBYTES:
					case SYM_UI24BEBYTES:
						if (ANY_BINSTR(next)) {
							ASSERT_UIBYTES_RANGE(next, 0xFFFFFF);
							count += (3 + VAL_LEN(next));
							continue;
						}
						goto error;
					case SYM_UI32BYTES:
					case SYM_UI32LEBYTES:
					case SYM_UI32BEBYTES:
						if (ANY_BINSTR(next)) {
							ASSERT_UIBYTES_RANGE(next, 0xFFFFFFFF);
							count += (4 + VAL_LEN(next));
							continue;
						}
						goto error;

					case SYM_ENCODEDU32:
					case SYM_ENCODEDU64:
						if (IS_INTEGER(next)) {
							count += EncodedU64_Size(VAL_UNT64(next));
							continue;
						}
						goto error;

					case SYM_VINT:
						if (IS_INTEGER(next)) {
							// max 8 bytes = 56 data bits, all-ones reserved (negative values are huge as unsigned)
							if (VAL_UNT64(next) >= (1ULL << 56) - 1) Trap1(RE_OUT_OF_RANGE, next);
							count += EncodedVINT_Size(VAL_UNT64(next));
							continue;
						}
						goto error;

					case SYM_FLOAT:
					case SYM_F32:
					case SYM_F32LE:
					case SYM_F32BE:
						if (IS_INTEGER(next) || IS_DECIMAL(next)) {
							count += 4;
							continue;
						}
						goto error;
					case SYM_DOUBLE:
					case SYM_F64:
					case SYM_F64LE:
					case SYM_F64BE:
						if (IS_INTEGER(next) || IS_DECIMAL(next)) {
							count += 8;
							continue;
						}
						goto error;
					case SYM_FLOAT16:
					case SYM_F16:
					case SYM_F16LE:
					case SYM_F16BE:
						if (IS_INTEGER(next) || IS_DECIMAL(next)) {
							count += 2;
							continue;
						}
						goto error;

					case SYM_UNIXTIME_NOW:
					case SYM_UNIXTIME_NOW_LE:
						value--; //there is no argument so no next
						count += 4;
						break;

					case SYM_MSDOS_DATE:
						if (!IS_DATE(next)) goto error; // only date allowed
						if (VAL_YEAR(next) < 1980 || VAL_YEAR(next) > 2107) Trap1(RE_OUT_OF_RANGE, next); // 7-bit year from 1980
						//continue..
					case SYM_MSDOS_TIME:
						if (IS_DATE(next) || IS_TIME(next)) {
							count += 2;
							continue;
						}
						goto error;

					case SYM_MSDOS_DATETIME:
						if (IS_DATE(next)) { // needs date part
							if (VAL_YEAR(next) < 1980 || VAL_YEAR(next) > 2107) Trap1(RE_OUT_OF_RANGE, next); // 7-bit year from 1980
							count += 4;
							continue;
						}
						goto error;

					case SYM_LENGTH:
						if (!IS_INTEGER(next)) goto error;
						// DER length (as read): 0 .. 0xFFFFFFFF
						if (VAL_INT64(next) < 0 || VAL_INT64(next) > (REBI64)0xFFFFFFFFLL) Trap1(RE_OUT_OF_RANGE, next);
						count += Der_Length_Size(VAL_UNT64(next));
						continue;
					case SYM_FIXED8:
					case SYM_FIXED16:
						if (!(IS_INTEGER(next) || IS_DECIMAL(next))) goto error;
						Bincode_Fixed_Value(cmd, next); // range check
						count += (cmd == SYM_FIXED8) ? 2 : 4;
						continue;
					case SYM_BITSET8:
					case SYM_BITSET16:
					case SYM_BITSET32:
						if (!IS_BITSET(next)) goto error;
						Bincode_Check_Bitset(next, BITSET_CMD_BYTES(cmd));
						count += BITSET_CMD_BYTES(cmd);
						continue;
					case SYM_STRING_BYTES:
					case SYM_OCTAL_BYTES:
						// fixed width field: <width> <value> (as read)
						if (!IS_INTEGER(next)) goto error;
						if (VAL_INT64(next) < (cmd == SYM_OCTAL_BYTES ? 1 : 0) || VAL_INT64(next) > 0x7FFFFFFF) Trap1(RE_OUT_OF_RANGE, next);
						len = VAL_INT32(next); // field width
						next = ++value;
						if (IS_GET_WORD(next)) next = Get_Var(next);
						else if (IS_GET_PATH(next)) {
							Do_Path(&next, NULL);
							next = DS_POP; // volatile stack reference
						}
						if (cmd == SYM_STRING_BYTES) {
							if (!ANY_BINSTR(next)) goto error;
							if (VAL_LEN(next) > len) Trap1(RE_OUT_OF_RANGE, next); // longer than the field
						} else {
							if (!IS_INTEGER(next)) goto error;
							if (VAL_INT64(next) < 0 || Octal_Digits(VAL_UNT64(next)) > len) Trap1(RE_OUT_OF_RANGE, next);
						}
						count += len;
						continue;
					case SYM_UNIXTIME:
					case SYM_UNIXTIME_LE:
						if (!IS_DATE(next)) goto error;
						si = Date_To_Timestamp(next);
						if (si < 0 || si > (REBI64)0xFFFFFFFFLL) Trap1(RE_OUT_OF_RANGE, next); // unsigned 32-bit (1970..2106)
						count += 4;
						continue;
					case SYM_VECTOR:
					case SYM_VECTOR_BE:
					case SYM_VECTOR_LE:
						if (!IS_VECTOR(next)) goto error;
						// byte order of struct elements is not defined
						if (cmd != SYM_VECTOR && VAL_VEC_IS_STRUCT(next)) Trap1(RE_INVALID_SPEC, next);
						if (VECTOR_BYTES(next) > 0x7FFFFFFF) Trap1(RE_SIZE_LIMIT, next);
						count += (REBCNT)VECTOR_BYTES(next);
						continue;
					case SYM_ALIGN:
						value--; // no argument (aligned above)
						continue;
					case SYM_BIT:
					case SYM_NOT_BIT:
						if (IS_LOGIC(next) || (IS_INTEGER(next) && (VAL_INT64(next) == 0 || VAL_INT64(next) == 1))) {
							if (++pb == 8) { count++; pb = 0; }
							continue;
						}
						goto error;
					case SYM_UB:
					case SYM_SB:
					case SYM_FB:
						if (!IS_INTEGER(next)) goto error;
						if (VAL_INT64(next) < 0 || VAL_INT64(next) > 64) Trap1(RE_OUT_OF_RANGE, next);
						j = VAL_INT32(next); // number of bits
						next = ++value;
						if (IS_GET_WORD(next)) next = Get_Var(next);
						else if (IS_GET_PATH(next)) {
							Do_Path(&next, NULL);
							next = DS_POP; // volatile stack reference
						}
						if (!(IS_INTEGER(next) || (cmd == SYM_FB && IS_DECIMAL(next)))) goto error;
						Bincode_Bits_Value(cmd, j, next); // range check
						pb += j;
						count += pb / 8;
						pb %= 8;
						continue;
					case SYM_RANDOM_BYTES:
						if (IS_INTEGER(next)) {
							if (VAL_INT64(next) < 0 || VAL_INT64(next) > 0x7FFFFFFF) Trap1(RE_OUT_OF_RANGE, next);
							count += VAL_INT32(next);
							continue;
						}
						goto error;
					default:
						value--;
						goto error;
					}
					break;
				case REB_SET_WORD:
					// optional INDEX (one-based, default) or INDEXZ (zero-based) word
					j = 1;
					next = value + 1;
					if (IS_WORD(next)) {
						if (VAL_WORD_CANON(next) == SYM_INDEXZ) { j = 0; value++; }
						else if (VAL_WORD_CANON(next) == SYM_INDEX) value++;
					}
					VAL_SET(temp, REB_INTEGER);
					SET_INT32(temp, count + j);
					Set_Var(data, temp);

					break;
				default:
					
					goto error;
				}
			} // end of for loop

			tail_new = count + (pb ? 1 : 0); //bytes needed to write (incl. partially written byte)
			if (tail > tail_new) tail_new = tail; // AT/SKIP may have moved back from a further position
			n = SERIES_TAIL(bin); // old tail
			//printf("index: %i tail: %i count: %i tail_new: %i rest: %i\n", index, tail, count, tail_new, SERIES_REST(bin));
			if (tail_new >= SERIES_REST(bin)) { // keep space for the terminator
				//puts("expanding");
				Expand_Series(bin, AT_TAIL, tail_new - SERIES_TAIL(bin));
			}

			if (tail_new > tail) tail = tail_new;
			SERIES_TAIL(bin) = tail;
			VAL_TAIL(buffer_read) = tail;
			// zero newly exposed bytes, so gaps (for example after AT past the tail) don't hold stale data
			if (tail > n) CLEAR(BIN_DATA(bin) + n, tail - n);
			

			value = VAL_BLK_DATA(val_write); //resets value at head of the input block

			//#########################################################
			//## Do real WRITE evaluation #############################
			//#########################################################
			
			cp = BIN_DATA(bin) + VAL_INDEX(buffer_write);

			for (; NOT_END(value); value++) {
				n = 0;
				switch(VAL_TYPE(value)) {
				case REB_GET_WORD:
					data = Get_Var(value);
					break;
				//case REB_GET_PATH:
				//	Do_Path(&value, NULL);
				//	value = DS_POP; // volatile stack reference
				//	break;
				default:
					data = value;
				}

				//printf("type: %i\n", VAL_TYPE(value), VAL_TYPE(data));

				switch (VAL_TYPE(data)) {
				case REB_BINARY:
				case REB_STRING:
				case REB_FILE:
				case REB_URL:
				case REB_EMAIL:
					if (outBit) { outBit = 0; cp++; VAL_INDEX(buffer_write)++; } // align
					n = VAL_LEN(data);
					//printf("write bytes: %i to #%0X\n", n, cp);
					memcpy(cp, VAL_BIN_AT(data), n);
					break;
				case REB_TUPLE:
					if (outBit) { outBit = 0; cp++; VAL_INDEX(buffer_write)++; } // align
					n = VAL_TUPLE_LEN(data);
					memcpy(cp, VAL_TUPLE(data), n);
					break;
				case REB_VECTOR:
					if (outBit) { outBit = 0; cp++; VAL_INDEX(buffer_write)++; } // align
					n = (REBCNT)VECTOR_BYTES(data);
					memcpy(cp, VECTOR_DATA_AT(data), n);
					break;
				case REB_STRUCT:
					if (outBit) { outBit = 0; cp++; VAL_INDEX(buffer_write)++; } // align
					n = VAL_STRUCT_SIZE(data);
					memcpy(cp, VAL_STRUCT_DATA_BIN(data), n);
					break;

				case REB_WORD:
					next = ++value;
					if (IS_GET_WORD(next)) next = Get_Var(next);
					else if (IS_GET_PATH(next)) {
						Do_Path(&next, NULL);
						next = DS_POP; // volatile stack reference
					}
					cmd = VAL_WORD_CANON(data);
					if (outBit && !IS_BINCODE_BIT_CMD(cmd)) { outBit = 0; cp++; VAL_INDEX(buffer_write)++; } // byte commands start at byte boundary
					if (IS_CHAR(next)) { SET_INTEGER(&char_arg, VAL_CHAR(next)); next = &char_arg; } // integer commands copy bytes of the 64-bit value
					switch (cmd) {
					case SYM_UI8:
						ASSERT_UI_RANGE(next, 0xFF);
					write_ui8:
						cp[0] = VAL_INT32(next) & 0xFF;
						n = 1;
						break;
					
					case SYM_UI16:
					case SYM_UI16BE:
						ASSERT_UI_RANGE(next, 0xFFFF);
					write_ui16:
						n = 2;
						i = VAL_INT32(next);
						bp = (REBYTE*)&i;
						WRITE_BE_2(cp, bp);
						break;
					case SYM_UI24:
					case SYM_UI24BE:
						ASSERT_UI_RANGE(next, 0xFFFFFF);
					write_ui24:
						n = 3;
						i = VAL_INT32(next);
						bp = (REBYTE*)&i;
						WRITE_BE_3(cp, bp);
						break;
					case SYM_UI32:
					case SYM_UI32BE:
						ASSERT_U32_RANGE(next);
					write_ui32:
						n = 4;
						bp = (REBYTE*)&VAL_INT64(next);
						WRITE_BE_4(cp, bp);
						break;
					case SYM_UI64:
					case SYM_UI64BE:
					write_ui64:
						n = 8;
						bp = (REBYTE*)&VAL_INT64(next);
						WRITE_BE_8(cp, bp);
						break;
					case SYM_UI16LE:
						ASSERT_UI_RANGE(next, 0xFFFF);
					write_ui16le:
						n = 2;
						bp = (REBYTE*)&VAL_INT64(next);
						WRITE_LE_2(cp, bp);
						break;
					case SYM_UI24LE:
						ASSERT_UI_RANGE(next, 0xFFFFFF);
					write_ui24le:
						n = 3;
						bp = (REBYTE*)&VAL_INT64(next);
						WRITE_LE_3(cp, bp);
						break;
					case SYM_UI32LE:
						ASSERT_U32_RANGE(next);
					write_ui32le:
						n = 4;
						bp = (REBYTE*)&VAL_INT64(next);
						WRITE_LE_4(cp, bp);
						break;
					case SYM_UI64LE:
					write_ui64le:
						n = 8;
						bp = (REBYTE*)&VAL_INT64(next);
						WRITE_LE_8(cp, bp);
						break;

					case SYM_SI8:
						ASSERT_SI_RANGE(next, 0x7F);
						goto write_ui8;
					case SYM_SI16:
					case SYM_SI16BE:
						ASSERT_SI_RANGE(next, 0x7FFF);
						goto write_ui16;
					case SYM_SI24:
					case SYM_SI24BE:
						ASSERT_SI_RANGE(next, 0x7FFFFF);
						goto write_ui24;
					case SYM_SI32:
					case SYM_SI32BE:
						ASSERT_SI_RANGE(next, 0x7FFFFFFF);
						goto write_ui32;
					case SYM_SI64:
					case SYM_SI64BE:
						goto write_ui64;
					case SYM_SI16LE:
						ASSERT_SI_RANGE(next, 0x7FFF);
						goto write_ui16le;
					case SYM_SI24LE:
						ASSERT_SI_RANGE(next, 0x7FFFFF);
						goto write_ui24le;
					case SYM_SI32LE:
						ASSERT_SI_RANGE(next, 0x7FFFFFFF);
						goto write_ui32le;
					case SYM_SI64LE:
						goto write_ui64le;

					case SYM_STRING:
						n = VAL_LEN(next);
						memcpy(cp, VAL_BIN_AT(next), n);
						cp[n++] = 0; // NUL terminator
						break;
					case SYM_BYTES:
						n = VAL_LEN(next);
						memcpy(cp, VAL_BIN_AT(next), n);
						break;
					case SYM_UI8BYTES:
						n = cp[0] = VAL_LEN(next);
						memcpy(++cp, VAL_BIN_AT(next), n);
						VAL_INDEX(buffer_write)++; //for the length byte;
						break;
					case SYM_UI16BYTES:
					case SYM_UI16BEBYTES:
						n = VAL_LEN(next);
						bp = (REBYTE*)&n;
						WRITE_BE_2(cp, bp);
						cp+=2;
						memcpy(cp, VAL_BIN_AT(next), n);
						VAL_INDEX(buffer_write)+=2; //for the length byte;
						break;
					case SYM_UI16LEBYTES:
						n = VAL_LEN(next);
						bp = (REBYTE*)&n;
						WRITE_LE_2(cp, bp);
						cp+=2;
						memcpy(cp, VAL_BIN_AT(next), n);
						VAL_INDEX(buffer_write)+=2; //for the length byte;
						break;

					case SYM_UI24BYTES:
					case SYM_UI24BEBYTES:
						n = VAL_LEN(next);
						bp = (REBYTE*)&n;
						WRITE_BE_3(cp, bp);
						cp += 3;
						memcpy(cp, VAL_BIN_AT(next), n);
						VAL_INDEX(buffer_write) += 3; //for the length byte;
						break;
					case SYM_UI24LEBYTES:
						n = VAL_LEN(next);
						bp = (REBYTE*)&n;
						WRITE_LE_3(cp, bp);
						cp += 3;
						memcpy(cp, VAL_BIN_AT(next), n);
						VAL_INDEX(buffer_write) += 3; //for the length byte;
						break;
					case SYM_UI32BYTES:
					case SYM_UI32BEBYTES:
						n = VAL_LEN(next);
						bp = (REBYTE*)&n;
						WRITE_BE_4(cp, bp);
						cp += 4;
						memcpy(cp, VAL_BIN_AT(next), n);
						VAL_INDEX(buffer_write) += 4; //for the length byte;
						break;
					case SYM_UI32LEBYTES:
						n = VAL_LEN(next);
						bp = (REBYTE*)&n;
						WRITE_LE_4(cp, bp);
						cp += 4;
						memcpy(cp, VAL_BIN_AT(next), n);
						VAL_INDEX(buffer_write) += 4; //for the length byte;
						break;

					case SYM_FLOAT:
					case SYM_F32:
					case SYM_F32LE:
						f32.v = (float)(IS_INTEGER(next) ? VAL_INT64(next) : VAL_DECIMAL(next));
						bp = (REBYTE *)&f32;
						WRITE_LE_4(cp, bp);
						n = 4;
						break;
					case SYM_F32BE:
						f32.v = (float)(IS_INTEGER(next) ? VAL_INT64(next) : VAL_DECIMAL(next));
						bp = (REBYTE *)&f32;
						WRITE_BE_4(cp, bp);
						n = 4;
						break;
					case SYM_DOUBLE:
					case SYM_F64:
					case SYM_F64LE:
						dbl = (REBDEC)(IS_INTEGER(next) ? VAL_INT64(next) : VAL_DECIMAL(next));
						n = 8;
						bp = (REBYTE *)&dbl;
						WRITE_LE_8(cp, bp);
						break;
					case SYM_F64BE:
						dbl = (REBDEC)(IS_INTEGER(next) ? VAL_INT64(next) : VAL_DECIMAL(next));
						n = 8;
						bp = (REBYTE *)&dbl;
						WRITE_BE_8(cp, bp);
						break;
					case SYM_FLOAT16:
					case SYM_F16:
					case SYM_F16BE:
					case SYM_F16LE:
						dbl = IS_INTEGER(next) ? (REBDEC)VAL_INT64(next) : VAL_DECIMAL(next);
						ushort = Double_To_Half(dbl);
						bp = (REBYTE *)&ushort;
						if (cmd == SYM_F16BE) {
							WRITE_BE_2(cp, bp);
						}
						else {
							WRITE_LE_2(cp, bp);
						}
						n = 2;
						break;

					case SYM_AT:
					case SYM_ATZ:
						VAL_INDEX(buffer_write) = VAL_INT32(next) - (cmd == SYM_AT ? 1 : 0);
						cp = BIN_DATA(bin) + VAL_INDEX(buffer_write);
						n = 0;
						break;

					case SYM_SKIP:
						// validated in the size pass
						VAL_INDEX(buffer_write) = (REBCNT)((REBI64)VAL_INDEX(buffer_write) + VAL_INT64(next));
						cp = BIN_DATA(bin) + VAL_INDEX(buffer_write);
						n = 0;
						break;
					case SYM_PAD:
						n = VAL_INDEX(buffer_write) % VAL_INT32(next);
						if (n > 0) {
							n = VAL_INT32(next) - n;
							memset(cp, 0, n);
						}
						break;

					case SYM_ENCODEDU32:
					case SYM_ENCODEDU64:
						if (cmd == SYM_ENCODEDU32) {
							ASSERT_U32_RANGE(next);
							u = (u64)VAL_UNT32(next);
						}
						else {
							u = VAL_UNT64(next);
						}
						n = 0;
						do {
							cp[n] = (char)(u & 0x7F);  // Take the lowest 7 bits of the value
							u >>= 7;                   // Shift the value right by 7 bits
							if (u != 0) cp[n] |= 0x80; // Set the continuation bit if there are more bytes to write
							n++;
						} while (u != 0);
						break;

					case SYM_VINT:
						u = VAL_UNT64(next);
						n = EncodedVINT_Size(u);
						for (i = n-1; i > 0; i--) {
							cp[i] = (char)(u & 0xFF);
							u >>= 8;
						}
						cp[0] = (char)(u | (0x80 >> (n-1)));
						break;

					case SYM_UNIXTIME_NOW:
						value--; // no args
						n = 4;
						i = (i32)time(NULL);
						bp = (REBYTE*)&i;
						WRITE_BE_4(cp, bp);
						break;
					case SYM_UNIXTIME_NOW_LE:
						value--; // no args
						n = 4;
						i = (i32)time(NULL);
						bp = (REBYTE*)&i;
						WRITE_LE_4(cp, bp);
						break;

					case SYM_MSDOS_DATE:
						MSDOS_PUT_U16(cp, Msdos_Date_Bits(next));
						n = 2;
						break;

					case SYM_MSDOS_TIME:
						MSDOS_PUT_U16(cp, Msdos_Time_Bits(next));
						n = 2;
						break;

					case SYM_MSDOS_DATETIME:
						MSDOS_PUT_U16(cp,     Msdos_Time_Bits(next));
						MSDOS_PUT_U16(cp + 2, Msdos_Date_Bits(next));
						n = 4;
						break;

					case SYM_LENGTH:
						// DER length, validated in the size pass
						u = VAL_UNT64(next);
						n = Der_Length_Size(u);
						if (n == 1) cp[0] = (REBYTE)u;
						else {
							cp[0] = (REBYTE)(0x80 | (n - 1));
							for (j = (REBINT)n - 1; j > 0; j--) { cp[j] = (REBYTE)u; u >>= 8; }
						}
						break;
					case SYM_FIXED8:
					case SYM_FIXED16:
						// little endian (as read), validated in the size pass
						si = Bincode_Fixed_Value(cmd, next);
						n = (cmd == SYM_FIXED8) ? 2 : 4;
						for (j = 0; j < (REBINT)n; j++) cp[j] = (REBYTE)((u64)si >> (8 * j));
						break;
					case SYM_BITSET8:
					case SYM_BITSET16:
					case SYM_BITSET32:
						// raw bitset bytes, zero padded (validated in the size pass)
						n = BITSET_CMD_BYTES(cmd);
						memset(cp, 0, n);
						memcpy(cp, BIN_DATA(VAL_SERIES(next)), MIN(n, SERIES_TAIL(VAL_SERIES(next))));
						break;
					case SYM_STRING_BYTES:
					case SYM_OCTAL_BYTES:
						// validated in the size pass; unused bytes are NUL
						n = VAL_INT32(next); // field width
						next = ++value;
						if (IS_GET_WORD(next)) next = Get_Var(next);
						else if (IS_GET_PATH(next)) {
							Do_Path(&next, NULL);
							next = DS_POP; // volatile stack reference
						}
						memset(cp, 0, n);
						if (cmd == SYM_STRING_BYTES) {
							memcpy(cp, VAL_BIN_AT(next), VAL_LEN(next));
						} else {
							// zero padded octal digits followed by NUL (like in TAR headers),
							// the NUL is omitted only when the value needs the whole field
							u = VAL_UNT64(next);
							len = (Octal_Digits(u) < n) ? n - 1 : n;
							for (i = len; i > 0; i--) { cp[i - 1] = (REBYTE)('0' + (u & 7)); u >>= 3; }
						}
						break;
					case SYM_UNIXTIME:
					case SYM_UNIXTIME_LE:
						ulong = (u32)Date_To_Timestamp(next); // range validated in the size pass
						if (cmd == SYM_UNIXTIME) {
							cp[0] = (REBYTE)(ulong >> 24); cp[1] = (REBYTE)(ulong >> 16); cp[2] = (REBYTE)(ulong >> 8); cp[3] = (REBYTE)ulong;
						} else {
							cp[0] = (REBYTE)ulong; cp[1] = (REBYTE)(ulong >> 8); cp[2] = (REBYTE)(ulong >> 16); cp[3] = (REBYTE)(ulong >> 24);
						}
						n = 4;
						break;
					case SYM_VECTOR:
					case SYM_VECTOR_BE:
					case SYM_VECTOR_LE:
						n = (REBCNT)VECTOR_BYTES(next);
						memcpy(cp, VECTOR_DATA_AT(next), n);
						if (Bincode_Swap_Needed(cmd)) Bincode_Swap_Endian(cp, n, SERIES_WIDE(VAL_SERIES(next)));
						break;
					case SYM_ALIGN:
						value--; // no argument (aligned above)
						break;
					case SYM_BIT:
					case SYM_NOT_BIT:
						u = IS_LOGIC(next) ? (VAL_LOGIC(next) ? 1 : 0) : (u64)VAL_INT64(next);
						if (cmd == SYM_NOT_BIT) u ^= 1;
						j = 1;
						goto write_bits;
					case SYM_UB:
					case SYM_SB:
					case SYM_FB:
						j = VAL_INT32(next); // number of bits (validated in the size pass)
						next = ++value;
						if (IS_GET_WORD(next)) next = Get_Var(next);
						else if (IS_GET_PATH(next)) {
							Do_Path(&next, NULL);
							next = DS_POP; // volatile stack reference
						}
						u = Bincode_Bits_Value(cmd, j, next);
					write_bits:
						// MSB first; outBit == 0 means no partially written byte
						for (; j > 0; j--) {
							if (outBit == 0) outBit = 0x80;
							if ((u >> (j - 1)) & 1) cp[0] |= outBit;
							else cp[0] &= ~outBit;
							outBit >>= 1;
							if (outBit == 0) {
								cp++;
								VAL_INDEX(buffer_write)++;
							}
						}
						n = 0;
						break;
					case SYM_RANDOM_BYTES:
						if (IS_INTEGER(next)) {
							n = (REBCNT)VAL_INT32(next);
							Random_Bytes(cp, n, TRUE);
						}
						break;
					default:
						Trap_Arg(value);
					}
					break;
				case REB_SET_WORD:
					// optional INDEX (one-based, default) or INDEXZ (zero-based) word
					j = 1;
					next = value + 1;
					if (IS_WORD(next)) {
						if (VAL_WORD_CANON(next) == SYM_INDEXZ) { j = 0; value++; }
						else if (VAL_WORD_CANON(next) == SYM_INDEX) value++;
					}
					VAL_SET(temp, REB_INTEGER);
					SET_INT32(temp, VAL_INDEX(buffer_write) + j);
					Set_Var(data, temp);
					continue;
				}
				cp += n;
				VAL_INDEX(buffer_write) += n;
			}

			w_mask = outBit;
			break; // end of REB_BLOCK write

		} //end of VAL_TYPE switch

		DS_DROP; // remove temp
	}

	if (ref_read) {	
		//printf("\nREADING... in-index: %i\n\n", VAL_INDEX(buffer_read));

		cp = BIN_DATA(bin) + VAL_INDEX(buffer_read);
		ep = BIN_DATA(bin) + VAL_TAIL(buffer_read);

		inBit = r_mask;

		if (IS_INTEGER(val_read)) {
			// negative counts are huge as unsigned; also avoids 64->32 bit truncation
			if (VAL_UNT64(val_read) > (REBU64)(ep - cp)) Trap1(RE_OUT_OF_RANGE, val_read);
			n = (REBCNT)VAL_INT64(val_read);
			ASSERT_READ_SIZE(val_read, cp, ep, n);
			if(ref_into) {
				Trap0(RE_FEATURE_NA);
			}
			Set_Binary(D_RET, Copy_Series_Part(bin, VAL_INDEX(buffer_read), n));
			VAL_TAIL(D_RET) = n;
			VAL_INDEX(buffer_read) += n;
			BINCODE_RETURN(R_RET);
		}
		if (ref_into && IS_STRUCT(val_into)) {
			// fill an existing struct in place (no new struct is made)
			if (!IS_STRUCT(val_read)) Trap_Arg(val_read);
			if (VAL_STRUCT_PROTECTED(val_into)) Trap0(RE_PROTECTED);
			if (IS_PROTECT_SERIES(VAL_STRUCT_DATA(val_into))) Trap0(RE_PROTECTED);
			n = VAL_STRUCT_SIZE(val_into);
			// the read spec must describe the same struct
			if (n != VAL_STRUCT_SIZE(val_read) || VAL_STRUCT_HASH(val_into) != VAL_STRUCT_HASH(val_read))
				Trap_Arg(val_into);
			ASSERT_READ_SIZE(val_read, cp, ep, n);
			COPY_MEM(VAL_STRUCT_DATA_BIN(val_into), cp, n);
			VAL_INDEX(buffer_read) += n;
			*D_RET = *val_into;
			BINCODE_RETURN(R_RET);
		}
		if (ref_into && IS_VECTOR(val_into)) {
			// fill an existing vector in place (type and length are given by the vector)
			cmd = IS_WORD(val_read) ? VAL_WORD_CANON(val_read) : 0;
			if (cmd != SYM_VECTOR && cmd != SYM_VECTOR_BE && cmd != SYM_VECTOR_LE) Trap_Arg(val_read);
			if (cmd != SYM_VECTOR && VAL_VEC_IS_STRUCT(val_into)) Trap_Arg(val_read);
			if (IS_PROTECT_SERIES(VAL_SERIES(val_into))) Trap0(RE_PROTECTED);
			if (VECTOR_BYTES(val_into) > (REBU64)(ep - cp)) Trap1(RE_OUT_OF_RANGE, val_read);
			n = (REBCNT)VECTOR_BYTES(val_into);
			bp = VECTOR_DATA_AT(val_into);
			COPY_MEM(bp, cp, n);
			if (Bincode_Swap_Needed(cmd)) Bincode_Swap_Endian(bp, n, SERIES_WIDE(VAL_SERIES(val_into)));
			VAL_INDEX(buffer_read) += n;
			*D_RET = *val_into;
			BINCODE_RETURN(R_RET);
		}
		as_block = IS_BLOCK(val_read);

		if(as_block) {
			// encoding is block, so we can use its values normally
			value = VAL_BLK_DATA(val_read);
			if (IS_END(value)) BINCODE_RETURN(R_NONE); //empty block
											  
			// create a block for results if needed
			if(ref_into) {
				blk = VAL_SERIES(val_into);
				if (IS_PROTECT_SERIES(blk)) Trap0(RE_PROTECTED);
			} else {
				blk = Make_Block(VAL_LEN(val_read));
			}
			Set_Block(ret, blk);
		} else {
			// if encoding is just a word, simulate block with single value on stack
			DS_PUSH(val_read);
			value = DS_TOP;
			if (IS_INTEGER(val_num)) DS_PUSH(val_num);
			DS_PUSH_END; // marks end of the block
			if(ref_into) {
				blk = VAL_SERIES(val_into);
				Set_Block(ret, blk);
			} else {
				SET_NONE(ret);
			}
		}

		DS_PUSH_NONE;
		temp = DS_TOP;
		REBINT ssp = DSP;  // starting stack pointer
		
		for (; NOT_END(value); value++) {
			n = 0;
			switch (VAL_TYPE(value)) {
				case REB_WORD:
					cmd = VAL_WORD_CANON(value);
					switch (cmd) {
						case SYM_UI8:
							n = 1;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, cp[0]);
							break;
						case SYM_SI8:
							n = 1;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, (i8)cp[0]);
							break;
						case SYM_UI16:
						case SYM_UI16BE:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, (u32)cp[1] + ((u32)cp[0] << 8));
							break;
						case SYM_SI16:
						case SYM_SI16BE:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, (i16)((i16)cp[1] + ((i16)cp[0] << 8)));
							break;
						case SYM_SI16LE:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, (i16)((i16)cp[0] + ((i16)cp[1] << 8)));
							break;
						case SYM_UI24:
						case SYM_UI24BE:
							n = 3;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, ((u32)cp[2] <<  0) |
											((u32)cp[1] <<  8) |
											((u32)cp[0] << 16));
							break;
						case SYM_SI24:
						case SYM_SI24BE:
							n = 3;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							i = ((i32)cp[2] <<  0) |
								((i32)cp[1] <<  8) |
								((i32)cp[0] << 16) ;
							if((cp[0] & 0x80) == 0x80) i |= 0xFF000000;
							SET_INT32(temp, i);
							break;
						case SYM_SI24LE:
							n = 3;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							i = ((i32)cp[0] <<  0) |
								((i32)cp[1] <<  8) |
								((i32)cp[2] << 16) ;
							if((cp[2] & 0x80) == 0x80) i |= 0xFF000000;
							SET_INT32(temp, i);
							break;
						case SYM_UI32:
						case SYM_UI32BE:
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							VAL_UNT64(temp) = ((u64)cp[3]      ) |
											  ((u64)cp[2] << 8 ) |
											  ((u64)cp[1] << 16) |
											  ((u64)cp[0] << 24) ;
							break;
						case SYM_UI32LE:
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							VAL_UNT64(temp) = ((u64)cp[0] << 0 ) |
											  ((u64)cp[1] << 8 ) |
											  ((u64)cp[2] << 16) |
											  ((u64)cp[3] << 24) ;
							break;
						case SYM_SI32:
						case SYM_SI32BE:
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, (i32)((u32)cp[3] << 0 ) |
											     ((u32)cp[2] << 8 ) |
											     ((u32)cp[1] << 16) |
											     ((u32)cp[0] << 24));
							break;
						case SYM_SI32LE:
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, (i32)((u32)cp[0] << 0 ) |
											     ((u32)cp[1] << 8 ) |
											     ((u32)cp[2] << 16) |
											     ((u32)cp[3] << 24));
							break;
						case SYM_SI64:
						case SYM_SI64BE:
						case SYM_UI64:
						case SYM_UI64BE:
							n = 8;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							VAL_UNT64(temp) =
								((u64)cp[7] << 0 ) |
								((u64)cp[6] << 8 ) |
								((u64)cp[5] << 16) |
								((u64)cp[4] << 24) |
								((u64)cp[3] << 32) |
								((u64)cp[2] << 40) |
								((u64)cp[1] << 48) |
								((u64)cp[0] << 56) ;
							break;
						case SYM_SI64LE:
						case SYM_UI64LE:
							n = 8;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							VAL_UNT64(temp) =
								((u64)cp[0] << 0 ) |
								((u64)cp[1] << 8 ) |
								((u64)cp[2] << 16) |
								((u64)cp[3] << 24) |
								((u64)cp[4] << 32) |
								((u64)cp[5] << 40) |
								((u64)cp[6] << 48) |
								((u64)cp[7] << 56) ;
							break;
						case SYM_LENGTH:
							ASSERT_READ_SIZE(value, cp, ep, 1);
							VAL_SET(temp, REB_INTEGER);
							if (cp[0] < 128) {
								// short form
								SET_INT32(temp, cp[0]);
							}
							else {
								// long form: low 7 bits = number of length bytes
								n = cp[0] & 127;
								if (n == 0) Trap1(RE_INVALID_SPEC, value);  // 0x80 = indefinite length (not DER)
								if (n > 4) Trap1(RE_OUT_OF_RANGE, value);
								ASSERT_READ_SIZE(value, cp, ep, n + 1);
								u = 0;
								for (i = 1; i <= n; i++) {
									u = (u << 8) | cp[i];
								}
								VAL_UNT64(temp) = u;
							}
							n++;
							break;
						case SYM_BITSET8:
							n = 1;
							goto readBitsetN;
						case SYM_BITSET16:
							n = 2;
							goto readBitsetN;
						case SYM_BITSET32:
							n = 4;
						readBitsetN:
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_BITSET);
							bin_new = Copy_Series_Part(bin, VAL_INDEX(buffer_read), n);
							VAL_SERIES(temp) = bin_new;
							VAL_INDEX(temp) = 0;
							break;
						case SYM_ENCODEDU32:
						case SYM_ENCODEDU64: {
							REBCNT shift = 0;
							REBYTE byte = 0;
							n = 0;
							u = 0;
							do {
								// max 10 bytes (64 bits); also covers 10-byte sign-extended negative int32 values
								if (n >= 10) Trap1(RE_OUT_OF_RANGE, value);
								ASSERT_READ_SIZE(value, cp, ep, n+1);
								byte = cp[n];
								u |= (u64)(byte & 0x7F) << shift;
								shift += 7;
								n++;
							} while ((byte & 0x80) != 0);
							VAL_SET(temp, REB_INTEGER);
							if (cmd == SYM_ENCODEDU32) u &= 0xFFFFFFFF;
							VAL_UNT64(temp) = u;
							break;
						}
						case SYM_UB:
						case SYM_SB:
						case SYM_FB:
							next = ++value;
							BINCODE_GET_ARG(next);
							if (!IS_INTEGER(next)) goto error_next_value;
							if (VAL_INT64(next) < 0 || VAL_INT64(next) > 64) Trap1(RE_OUT_OF_RANGE, next);
							j = VAL_INT32(next); // number of bits
							u = 0;
							if (j > 0) {
								if (inBit == 0) inBit = 0x80;
								for (nbits = (REBCNT)j; nbits > 0; nbits--) {
									ASSERT_READ_SIZE(value, cp, ep, 1);
									u <<= 1;
									if (IS_BIT_SET(cp[0], inBit)) u |= 1;
									NEXT_IN_BIT(inBit);
								}
								STORE_IN_BIT(val_ctx, inBit);
								if (cmd != SYM_UB && j < 64) {
									// sign extend: http://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend
									u64 m = (u64)1 << (j - 1); // sign bit mask
									u = (u ^ m) - m;
								}
							}
							if (cmd == SYM_FB) {
								SET_DECIMAL(temp, ((double)((i64)u) / 65536.0));
							} else {
								VAL_SET(temp, REB_INTEGER);
								VAL_UNT64(temp) = u;
							}
							break;
						case SYM_BIT:
						case SYM_NOT_BIT:
							ASSERT_READ_SIZE(value, cp, ep, 1);
							//printf("inbit: %i cp: %i\n", inBit, cp[0]);
							if(inBit == 0) inBit = 0x80;
							VAL_SET(temp, REB_LOGIC);
							if(VAL_WORD_CANON(value) == SYM_BIT) {
								VAL_LOGIC(temp) = (cp[0] & inBit) == inBit ? TRUE : FALSE;
							} else {
								VAL_LOGIC(temp) = (cp[0] & inBit) == inBit ? FALSE : TRUE;
							}
							inBit = inBit >> 1;
							if(inBit == 0) n++;
							STORE_IN_BIT(val_ctx, inBit);
							break;
						case SYM_ALIGN:
							// aligns bit buffer to byte boundary
							//if (inBit == 128) inBit = 0;
							if (inBit > 0 && inBit < 128) {
								inBit = 0;
								cp++;
								VAL_INDEX(buffer_read)++;
								STORE_IN_BIT(val_ctx, inBit);
							}
							continue;
						case SYM_UI16LE:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, cp[0] + (cp[1] << 8));
							break;
						case SYM_UI24LE:
							n = 3;
							ASSERT_READ_SIZE(value, cp, ep, n);
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, cp[0] + (cp[1] << 8) + (cp[2] << 16));
							break;
						case SYM_BYTES:
						case SYM_OCTAL_BYTES:
						case SYM_STRING_BYTES:
							//puts("bytes");
							next = ++value;
							BINCODE_GET_ARG(next);
							if (IS_END(next)) {
								// reads all bytes to tail of the buffer
								// used in case like: bincode/read b 'bytes
								n = VAL_TAIL(buffer_read) - VAL_INDEX(buffer_read);
								value--;
							} else {
								if (!IS_INTEGER(next)) Trap1(RE_INVALID_SPEC, value);
								if (VAL_UNT64(next) > (REBU64)(ep - cp)) Trap1(RE_OUT_OF_RANGE, value);
								n = VAL_INT32(next);
							}
							ASSERT_READ_SIZE(value, cp, ep, n);
						readNBytes:
							// WARNING: this piece of code is also used for SYM_UI*BYTES commands! 
							if (cmd == SYM_OCTAL_BYTES) {
								// octal number
								u = 0;
								i = 0;
								while ((i < n) && cp[i]) {
									u = (u << 3) | (u64)(cp[i++] - '0');
								}
								SET_INTEGER(temp, u);
							} else {
								VAL_SET(temp, (cmd == SYM_STRING_BYTES) ? REB_STRING: REB_BINARY);
								bin_new = Copy_Series_Part(bin, VAL_INDEX(buffer_read), n);
								VAL_SERIES(temp) = bin_new;
								VAL_INDEX(temp) = 0;
								if (cmd == SYM_STRING_BYTES) {
									VAL_TAIL(temp) = (REBCNT)strnlen(cs_cast(VAL_BIN(temp)), n);
								}
							}
							break;
						case SYM_STRING:
							bp = cp;
							for(;;) {
								if (bp >= ep) Trap1(RE_OUT_OF_RANGE, value); // no NUL before end
								if (bp[0] == 0) break;
								bp++;
							}
							n = (REBCNT)(bp - cp);
							VAL_SET(temp, REB_STRING);
							bin_new = Copy_Series_Part(bin, VAL_INDEX(buffer_read), n);
							SET_STR_END(bin_new, n);
							VAL_SERIES(temp) = bin_new;
							VAL_INDEX(temp) = 0;
							n++;
							break;
						case SYM_UI8BYTES:
							//puts("ui8bytes");
							ASSERT_READ_SIZE(value, cp, ep, 1);
							n = cp[0];
							ASSERT_READ_SIZE(value, cp, ep, n);
							cp++;
							VAL_INDEX(buffer_read)++;
							goto readNBytes;
						case SYM_UI16BYTES:
						case SYM_UI16BEBYTES:
							//puts("ui16bytes");
							ASSERT_READ_SIZE(value, cp, ep, 2);
							n = (cp[0] << 8) + cp[1];
							ASSERT_READ_SIZE(value, cp, ep, n);
							cp += 2;
							VAL_INDEX(buffer_read) += 2;
							goto readNBytes;
						case SYM_UI16LEBYTES:
							ASSERT_READ_SIZE(value, cp, ep, 2);
							n = (cp[1] << 8) + cp[0];
							ASSERT_READ_SIZE(value, cp, ep, n);
							cp += 2;
							VAL_INDEX(buffer_read) += 2;
							goto readNBytes;
						case SYM_UI24BYTES:
						case SYM_UI24BEBYTES:
							//puts("ui16bytes");
							ASSERT_READ_SIZE(value, cp, ep, 3);
							n = cp[2] + (cp[1] << 8) + (cp[0] << 16);
							ASSERT_READ_SIZE(value, cp, ep, n);
							cp += 3;
							VAL_INDEX(buffer_read) += 3;
							goto readNBytes;
						case SYM_UI24LEBYTES:
							//puts("ui16bytes");
							ASSERT_READ_SIZE(value, cp, ep, 3);
							n = cp[0] + (cp[1] << 8) + (cp[2] << 16);
							ASSERT_READ_SIZE(value, cp, ep, n);
							cp += 3;
							VAL_INDEX(buffer_read) += 3;
							goto readNBytes;
						case SYM_UI32BYTES:
						case SYM_UI32BEBYTES:
							ASSERT_READ_SIZE(value, cp, ep, 4);
							n = (REBCNT)(
								((u64)cp[3]      ) |
								((u64)cp[2] << 8 ) |
								((u64)cp[1] << 16) |
								((u64)cp[0] << 24));
							ASSERT_READ_SIZE(value, cp, ep, n);
							cp += 4;
							VAL_INDEX(buffer_read) += 4;
							goto readNBytes;
						case SYM_UI32LEBYTES:
							ASSERT_READ_SIZE(value, cp, ep, 4);
							n = (REBCNT)(
								((u64)cp[0]      ) |
								((u64)cp[1] << 8 ) |
								((u64)cp[2] << 16) |
								((u64)cp[3] << 24));
							ASSERT_READ_SIZE(value, cp, ep, n);
							cp += 4;
							VAL_INDEX(buffer_read) += 4;
							goto readNBytes;
						case SYM_AT:
						case SYM_ATZ:
							// uses absolute positioning from series HEAD!
							next = ++value;
							BINCODE_GET_ARG(next);
							if (!IS_INTEGER(next)) goto error_next_value;
							i = VAL_INT32(next) - (cmd == SYM_AT ? 1 : 0);
							ASSERT_INDEX_RANGE(buffer_read, i, value);
							VAL_INDEX(buffer_read) = i;
							cp = BIN_DATA(bin) + VAL_INDEX(buffer_read);
							continue;
						case SYM_INDEX:
						case SYM_INDEXZ:
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, VAL_INDEX(buffer_read) + (cmd == SYM_INDEX ? 1 : 0));
							n = 0;
							break;
						case SYM_SKIP:
							next = ++value;
							BINCODE_GET_ARG(next);
							if (!IS_INTEGER(next)) goto error_next_value;
							i = VAL_INDEX(buffer_read) + VAL_INT32(next);
							ASSERT_INDEX_RANGE(buffer_read, i, value);
							VAL_INDEX(buffer_read) = i; //TODO: range test
							cp = BIN_DATA(bin) + VAL_INDEX(buffer_read);
							continue;
						case SYM_PAD:
							next = ++value;
							BINCODE_GET_ARG(next);
							if (!IS_INTEGER(next)) goto error_next_value;
							if (VAL_INT32(next) <= 0) Trap1(RE_INVALID_ARG, next);
							i = VAL_INDEX(buffer_read) % VAL_INT32(next);
							if (i > 0) {
								i = VAL_INDEX(buffer_read) + VAL_INT32(next) - i; // new index
								ASSERT_INDEX_RANGE(buffer_read, i, value);
								VAL_INDEX(buffer_read) = i;
								cp = BIN_DATA(bin) + VAL_INDEX(buffer_read);
							}
							continue;
						case SYM_SKIPBITS:
							next = ++value;
							BINCODE_GET_ARG(next);
							if (!IS_INTEGER(next)) goto error_next_value;
							i = VAL_INT32(next);
							if (i >= 8) {
								i /= 8;
								//printf("byte skip: %d\n", i);
								ASSERT_READ_SIZE(value, cp, ep, i);
								cp += i;
								VAL_INDEX(buffer_read) += i;
								i = VAL_INT32(next) - (i * 8);
							}
							if (inBit == 0) inBit = 0x80;
							while (i > 0) {
								i--;
								//printf("inbit %d: %d %d\n",i, inBit, VAL_INDEX(buffer_read));
								NEXT_IN_BIT(inBit);
							}
							STORE_IN_BIT(val_ctx, inBit);
							continue;
						case SYM_LENGTHQ:
							VAL_SET(temp, REB_INTEGER);
							SET_INT32(temp, VAL_TAIL(buffer_read) - VAL_INDEX(buffer_read));
							break;
						case SYM_TUPLE3:
							n = 3;
							goto readNTuple;
						case SYM_TUPLE4:
							n = 4;
						readNTuple:
							ASSERT_READ_SIZE(value, cp, ep, n);
							Set_Tuple(temp, BIN_DATA(bin) + VAL_INDEX(buffer_read), n);
							break;
						case SYM_FLOAT16:
						case SYM_F16:
						case SYM_F16LE:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							f16.bytes.low  = cp[0];
							f16.bytes.high = cp[1];
							SET_DECIMAL(temp, float16to32(f16) );
							break;
						case SYM_F16BE:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							f16.bytes.low = cp[1];
							f16.bytes.high = cp[0];
							SET_DECIMAL(temp, float16to32(f16));
							break;
						case SYM_FLOAT:
						case SYM_F32:
						case SYM_F32LE:
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							memcpy(&f32.v, cp, 4); // cp may be unaligned
							SET_DECIMAL(temp, f32.v);
							break;
						case SYM_F32BE:
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							buf[0] = cp[3];
							buf[1] = cp[2];
							buf[2] = cp[1];
							buf[3] = cp[0];
							memcpy(&f32.v, buf, 4);
							SET_DECIMAL(temp, f32.v);
							break;
						case SYM_DOUBLE:
						case SYM_F64:
						case SYM_F64LE:
							n = 8;
							ASSERT_READ_SIZE(value, cp, ep, n);
							memcpy(&dbl, cp, 8); // cp may be unaligned
							SET_DECIMAL(temp, dbl);
							break;
						case SYM_F64BE:
							n = 8;
							ASSERT_READ_SIZE(value, cp, ep, n);
							buf[0] = cp[7];
							buf[1] = cp[6];
							buf[2] = cp[5];
							buf[3] = cp[4];
							buf[4] = cp[3];
							buf[5] = cp[2];
							buf[6] = cp[1];
							buf[7] = cp[0];
							memcpy(&dbl, buf, 8);
							SET_DECIMAL(temp, dbl);
							break;
						case SYM_FIXED8:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							// signed 8.8 (`i` is unsigned, so the value is converted directly)
							SET_DECIMAL(temp, (REBDEC)(i16)((u16)cp[0] | ((u16)cp[1] << 8)) / 256.0);
							break;
						case SYM_FIXED16:
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							// signed 16.16 (`i` is unsigned, so the value is converted directly)
							SET_DECIMAL(temp, (REBDEC)(i32)((u32)cp[0] | ((u32)cp[1] << 8) | ((u32)cp[2] << 16) | ((u32)cp[3] << 24)) / 65536.0);
							break;
						case SYM_MSDOS_DATETIME:
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							msdos_bits = MSDOS_GET_U16(cp + 2); // date
							VAL_SET  (temp, REB_DATE);
							VAL_YEAR (temp) = ((msdos_bits >> 9) & 0x7F) + 1980;
							VAL_MONTH(temp) = (msdos_bits >> 5) & 0x0F;
							VAL_DAY  (temp) = msdos_bits & 0x1F;
							VAL_ZONE (temp) = 0;
							msdos_bits = MSDOS_GET_U16(cp);     // time
							VAL_TIME (temp) = TIME_SEC(MSDOS_TIME_SECS(msdos_bits));
							break;
						case SYM_MSDOS_DATE:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							msdos_bits = MSDOS_GET_U16(cp);
							CLEARS(temp);
							VAL_SET  (temp, REB_DATE);
							VAL_YEAR (temp) = ((msdos_bits >> 9) & 0x7F) + 1980;
							VAL_MONTH(temp) = (msdos_bits >> 5) & 0x0F;
							VAL_DAY  (temp) = msdos_bits & 0x1F;
							VAL_TIME (temp) = NO_TIME;
							break;
						case SYM_MSDOS_TIME:
							n = 2;
							ASSERT_READ_SIZE(value, cp, ep, n);
							msdos_bits = MSDOS_GET_U16(cp);
							CLEARS(temp);
							VAL_SET  (temp, REB_TIME);
							VAL_TIME (temp) = TIME_SEC(MSDOS_TIME_SECS(msdos_bits));
							break;
						case SYM_CROP:
							n = 0;
							if (IS_PROTECT_SERIES(bin)) Trap0(RE_PROTECTED);
							Remove_Series(VAL_SERIES(buffer_read), 0, VAL_INDEX(buffer_read));
							cp = VAL_BIN_HEAD(buffer_read);
							ep = cp + VAL_TAIL(buffer_read); // head may move and tail shrinks
							VAL_INDEX(buffer_write) = MAX(0, (REBI64)VAL_INDEX(buffer_write) - VAL_INDEX(buffer_read));
							VAL_INDEX(buffer_read) = 0;
							continue;

						case SYM_VINT: {
							// A variable-length format for positive integers
							ASSERT_READ_SIZE(value, cp, ep, 1);
							int mask = 0x80;
							n = 1;
							// Determine the length of the VINT
							while (mask) {
								if (cp[0] & mask) break;
								mask >>= 1;
								n++;
							}
							if (n > 8) Trap1(RE_INVALID_SPEC, value); // first byte 0x00 is not a valid VINT
							// Extract the VINT value
							ASSERT_READ_SIZE(value, cp, ep, n);
							u = (u64)(cp[0] & (0xFF >> n));
    						for (REBCNT i = 1; i < n; i++) {
								u = (u << 8) | cp[i];
							}
							VAL_SET(temp, REB_INTEGER);
							VAL_UNT64(temp) = u;
							break;
						}

						case SYM_UNIXTIME:
						case SYM_UNIXTIME_LE:
							// unsigned 32-bit unix timestamp (1970..2106) as UTC date
							n = 4;
							ASSERT_READ_SIZE(value, cp, ep, n);
							if (cmd == SYM_UNIXTIME)
								ulong = ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | (u32)cp[3];
							else
								ulong = (u32)cp[0] | ((u32)cp[1] << 8) | ((u32)cp[2] << 16) | ((u32)cp[3] << 24);
							Timestamp_To_Date(temp, (REBI64)ulong);
							break;
						case SYM_VECTOR:
						case SYM_VECTOR_BE:
						case SYM_VECTOR_LE: {
							// [vector <type> <count>], type: vector type word, registered struct name or a struct
							REBVAL *vtype;
							REBSER *fields = NULL;
							REBCNT vt = 0, wide;
							next = ++value;
							BINCODE_GET_ARG(next);
							vtype = next;
							if (IS_WORD(vtype)) {
								REBCNT sym = Normalize_Vector_Type_Symbol(VAL_WORD_CANON(vtype));
								if (sym >= SYM_INT8X && sym <= SYM_FLOAT64X) {
									vt = sym - SYM_INT8X; // same mapping as in t-vector.c
								} else {
									REBVAL *spec = Find_Struct_Spec(vtype);
									if (!spec) goto error_next_value;
									fields = VAL_SERIES(spec)->series;
								}
							}
							else if (IS_STRUCT(vtype)) fields = VAL_STRUCT_FIELDS(vtype);
							else goto error_next_value;
							next = ++value;
							BINCODE_GET_ARG(next);
							if (!IS_INTEGER(next)) goto error_next_value;
							if (VAL_INT64(next) < 0 || VAL_INT64(next) > 0x7FFFFFFF) Trap1(RE_OUT_OF_RANGE, next);
							j = VAL_INT32(next); // number of values
							if (fields) {
								if (cmd != SYM_VECTOR) Trap1(RE_INVALID_SPEC, value); // byte order of struct elements is not defined
								if (FIELDS_NEED_MARK(fields)) Trap0(RE_PROTECTED);
								wide = FIELDS_INFO(fields)->size;
							} else {
								wide = VECT_WIDE(vt);
							}
							// validate the size before allocation (the count usually comes from the data)
							if ((REBU64)j * wide > (REBU64)(ep - cp)) Trap1(RE_OUT_OF_RANGE, value);
							if (!(fields ? Make_Vector_Struct(temp, fields, j, 1) : Make_Vector(temp, vt, j, 1)))
								Trap1(RE_INVALID_SPEC, vtype);
							n = j * wide;
							COPY_MEM(SERIES_DATA(VAL_SERIES(temp)), cp, n);
							if (Bincode_Swap_Needed(cmd)) Bincode_Swap_Endian(SERIES_DATA(VAL_SERIES(temp)), n, wide);
							break;
						}
						default:
							// registered struct name, e.g.: register point3d!: make struct! [...]
							if (!Find_Struct_Spec(value)) Trap1(RE_INVALID_SPEC, value);
							{
								REBVAL spec[2]; // virtual block: [name] (no initial values)
								spec[0] = *value;
								SET_END(&spec[1]);
								if (!MT_Struct(temp, spec, REB_STRUCT)) Trap1(RE_INVALID_SPEC, value);
							}
							if (VAL_STRUCT_PROTECTED(temp)) Trap0(RE_PROTECTED);
							n = VAL_STRUCT_SIZE(temp);
							ASSERT_READ_SIZE(value, cp, ep, n);
							COPY_MEM(VAL_STRUCT_DATA_BIN(temp), cp, n);
							break;
					}
					break;
				case REB_SET_WORD:
					//puts("-- found set-word?");
					DS_PUSH(value);
					continue;
				case REB_GET_WORD:
				case REB_GET_PATH:
					data = value;
					BINCODE_GET_ARG(data);
					if (!IS_STRUCT(data)) Trap1(RE_INVALID_SPEC, value);
					goto read_struct;
				case REB_STRUCT:
					// read struct data into a new copy of the given struct
					data = value;
				read_struct:
					if (VAL_STRUCT_PROTECTED(data)) Trap0(RE_PROTECTED);
					n = VAL_STRUCT_SIZE(data);
					ASSERT_READ_SIZE(value, cp, ep, n);
					Copy_Struct_Value(data, temp);
					COPY_MEM(VAL_STRUCT_DATA_BIN(temp), cp, n);
					break;
				case REB_BINARY:
					// do binary comparison, return TRUE and skip if data matches
					// or return FALSE and keep position where it is
					VAL_SET(temp, REB_LOGIC);
					n = Val_Series_Len(value);
					if ((cp + n) > ep || NZ(memcmp(cp, VAL_BIN_DATA(value), n))) {
						VAL_LOGIC(temp) = FALSE;
						n = 0; // no advance
					}
					else {
						VAL_LOGIC(temp) = TRUE;
					}
					break;
				default:
					Trap1(RE_INVALID_SPEC, value);
					//or...
					//*temp = *value; // ...pass value as a result
			}
			// Set prior set-words:
			while (DSP > ssp) {
				Set_Var(DS_TOP, temp);
				DS_DROP;
			}
			
			if (ref_into) {
				Insert_Series(blk, VAL_INDEX(val_into), (REBYTE*)temp, 1);
				VAL_INDEX(val_into)++;
			} else {
				if (as_block) {
					Append_Val(blk, temp);
				}
			}
			
			cp += n;
			VAL_INDEX(buffer_read) += n;
		}
		
		if (as_block) {
			DS_DROP; // temp
		} else {
			*ret = *temp;
			DS_DROP; // temp
			DS_DROP; // END of the virtual block
			DS_DROP; // value
			if (IS_INTEGER(val_num)) DS_DROP;
			//@@ could above be done better?
		}

		if(ref_into) *ret = *val_into;

		BINCODE_RETURN(R_RET);
	}

	//printf("DS_TOP end: %x\n", DS_TOP);
	BINCODE_RETURN(R_ARG1);

error:
	if (IS_END(value)) value--; // missing argument at block end: report the command
	Trap_Word(RE_DIALECT, SYM_BINCODE, value);
error_next_value:
	if (IS_END(next)) value--;
	Trap1(RE_INVALID_SPEC, value);
}
#endif //IGNORE_BINCODE