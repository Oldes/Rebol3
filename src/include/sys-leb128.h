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
**  Summary: Unsigned LEB128 (variable length integer) support
**  Module:  sys-leb128.h
**  Author:  Oldes
**  Notes:
**    The number is stored in groups of 7 bits, the least significant
**    group first. Each byte except the last one has its top bit set.
**
**    Used by Bincode (ENCODEDU32 and ENCODEDU64) and by Rebin.
**
**    Signed numbers are stored as their 64-bit two's complement, so
**    a negative number always takes 10 bytes.
**
***********************************************************************/

#ifndef SYS_LEB128_H
#define SYS_LEB128_H

#define LEB128_MAX_SIZE 10 // number of bytes needed for any 64-bit value


/***********************************************************************
**
*/	static FORCE_INLINE REBCNT Size_LEB128(REBU64 value)
/*
**		Returns the number of bytes needed to store the value.
**
***********************************************************************/
{
	REBCNT count = 1;
	while ((value >>= 7) != 0) count++;
	return count;
}


/***********************************************************************
**
*/	static FORCE_INLINE REBCNT Write_LEB128(REBYTE *cp, REBU64 value)
/*
**		Writes the value and returns the number of written bytes.
**		There must be a space for Size_LEB128(value) bytes!
**
***********************************************************************/
{
	REBCNT n = 0;
	do {
		cp[n] = (REBYTE)(value & 0x7F);
		value >>= 7;
		if (value != 0) cp[n] |= 0x80; // more bytes follow
		n++;
	} while (value != 0);
	return n;
}


/***********************************************************************
**
*/	static FORCE_INLINE REBCNT Read_LEB128(const REBYTE *cp, const REBYTE *ep, REBU64 *value)
/*
**		Reads a value from the data at `cp`, which end at `ep`.
**		Returns the number of bytes used, or 0 when the data end
**		before the number does, or when the number is longer than
**		10 bytes (it would not fit into 64 bits).
**
***********************************************************************/
{
	REBU64 num = 0;
	REBCNT n = 0;
	REBYTE byte;

	do {
		if (n >= LEB128_MAX_SIZE || cp + n >= ep) return 0;
		byte = cp[n];
		num |= (REBU64)(byte & 0x7F) << (7 * n);
		n++;
	} while (byte & 0x80);

	*value = num;
	return n;
}

#endif // SYS_LEB128_H
