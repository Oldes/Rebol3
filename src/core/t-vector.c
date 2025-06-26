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
**  Module:  t-vector.c
**  Summary: vector datatype
**  Section: datatypes
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/

#include "sys-core.h"

static const REBCNT normalized_vect_sym[29] = {
	SYM_INT8X,     //SYM_INT8X
	SYM_INT16X,    //SYM_INT16X
	SYM_INT32X,    //SYM_INT32X
	SYM_INT64X,    //SYM_INT64X
	SYM_UINT8X,    //SYM_UINT8X
	SYM_UINT16X,   //SYM_UINT16X
	SYM_UINT32X,   //SYM_UINT32X
	SYM_UINT64X,   //SYM_UINT64X
	SYM_FLOAT8X,   //SYM_FLOAT8X
	SYM_FLOAT16X,  //SYM_FLOAT16X
	SYM_FLOAT32X,  //SYM_FLOAT32X
	SYM_FLOAT64X,  //SYM_FLOAT64X
	SYM_INT8X,     //SYM_I8X
	SYM_INT16X,    //SYM_I16X
	SYM_INT32X,    //SYM_I32X
	SYM_INT64X,    //SYM_I64X
	SYM_UINT8X,    //SYM_U8X
	SYM_UINT16X,   //SYM_U16X
	SYM_UINT32X,   //SYM_U32X
	SYM_UINT64X,   //SYM_U64X
	SYM_FLOAT8X,   //SYM_F8X
	SYM_FLOAT16X,  //SYM_F16X
	SYM_FLOAT32X,  //SYM_F32X
	SYM_FLOAT64X,  //SYM_F64X
	SYM_UINT8X,    //SYM_BYTEX
	SYM_FLOAT16X,  //SYM_HALFX
	SYM_FLOAT32X,  //SYM_FLOATX
	SYM_FLOAT32X,  //SYM_SINGLEX
	SYM_FLOAT64X,  //SYM_DOUBLEX

};

/***********************************************************************
**
*/	REBCNT Normalize_Vector_Type_Symbol(REBCNT sym)
/*
**		Return normalized symbol from an numeric vector type alias.
**
***********************************************************************/
{
	if (sym < SYM_INT8X || sym > SYM_DOUBLEX) return sym;
	return normalized_vect_sym[sym - SYM_INT8X];
}

REBU64 f_to_u64(float n) {
	union {
		REBU64 u;
		REBDEC d;
	} t;
	t.d = n;
	return t.u;
}


typedef void (*SetterFunc)(const void *data, REBCNT n, REBVAL *val);
typedef void (*GetterFunc)(const void *data, REBCNT n, REBVAL *val);

static void set_i8(const void *data, REBCNT n, REBVAL *val) { ((i8 *)data)[n] = (i8)VAL_INT64(val); }
static void set_i16(const void *data, REBCNT n, REBVAL *val) { ((i16 *)data)[n] = (i16)VAL_INT64(val); }
static void set_i32(const void *data, REBCNT n, REBVAL *val) { ((i32 *)data)[n] = (i32)VAL_INT64(val); }
static void set_i64(const void *data, REBCNT n, REBVAL *val) { ((i64 *)data)[n] = VAL_INT64(val); }
static void set_u8(const void *data, REBCNT n, REBVAL *val) { ((u8 *)data)[n] = (u8)VAL_UNT64(val); }
static void set_u16(const void *data, REBCNT n, REBVAL *val) { ((u16 *)data)[n] = (u16)VAL_UNT64(val); }
static void set_u32(const void *data, REBCNT n, REBVAL *val) { ((u32 *)data)[n] = (u32)VAL_UNT64(val); }
static void set_u64(const void *data, REBCNT n, REBVAL *val) { ((u64 *)data)[n] = VAL_UNT64(val); }
static void set_float(const void *data, REBCNT n, REBVAL *val) { ((float *)data)[n] = (float)VAL_DECIMAL(val); }
static void set_double(const void *data, REBCNT n, REBVAL *val) { ((double *)data)[n] = VAL_DECIMAL(val); }

static void get_i8(const void *data, REBCNT n, REBVAL *val)  { VAL_INT64(val) = ((i8 *)data)[n]; }
static void get_i16(const void *data, REBCNT n, REBVAL *val) { VAL_INT64(val) = ((i16 *)data)[n]; }
static void get_i32(const void *data, REBCNT n, REBVAL *val) { VAL_INT64(val) = ((i32 *)data)[n]; }
static void get_i64(const void *data, REBCNT n, REBVAL *val) { VAL_INT64(val) = ((i64 *)data)[n]; }
static void get_u8(const void *data, REBCNT n, REBVAL *val)  { VAL_UNT64(val) = ((u8 *)data)[n]; }
static void get_u16(const void *data, REBCNT n, REBVAL *val) { VAL_UNT64(val) = ((u16 *)data)[n]; }
static void get_u32(const void *data, REBCNT n, REBVAL *val) { VAL_UNT64(val) = ((u32 *)data)[n]; }
static void get_u64(const void *data, REBCNT n, REBVAL *val) { VAL_UNT64(val) = ((u64 *)data)[n]; }
static void get_float(const void *data, REBCNT n, REBVAL *val)  { VAL_UNT64(val) = f_to_u64(((float *)data)[n]); }
static void get_double(const void *data, REBCNT n, REBVAL *val) { VAL_UNT64(val) = ((REBU64 *)data)[n]; }

// Comparison functions for qsort
typedef int(*CompareFunc)(const void *a, const void *b);
#define COMP_FUNC_BODY(type) {             \
    type fa = *(const type *)a;   \
	type fb = *(const type *)b;   \
    return (fa > fb) - (fa < fb); \
}
// (ascending order)
static int cmp_i8(const void *a, const void *b) { COMP_FUNC_BODY(i8) }
static int cmp_i16(const void *a, const void *b) { COMP_FUNC_BODY(i16) }
static int cmp_i32(const void *a, const void *b) { COMP_FUNC_BODY(i32) }
static int cmp_i64(const void *a, const void *b) { COMP_FUNC_BODY(i64) }
static int cmp_u8(const void *a, const void *b) { COMP_FUNC_BODY(u8) }
static int cmp_u16(const void *a, const void *b) { COMP_FUNC_BODY(u16) }
static int cmp_u32(const void *a, const void *b) { COMP_FUNC_BODY(u32) }
static int cmp_u64(const void *a, const void *b) { COMP_FUNC_BODY(u64) }
static int cmp_float(const void *a, const void *b) { COMP_FUNC_BODY(float) }
static int cmp_double(const void *a, const void *b) { COMP_FUNC_BODY(double) }
// reversed...
static int cmp_i8_rev(const void *b, const void *a) { COMP_FUNC_BODY(i8) }
static int cmp_i16_rev(const void *b, const void *a) { COMP_FUNC_BODY(i16) }
static int cmp_i32_rev(const void *b, const void *a) { COMP_FUNC_BODY(i32) }
static int cmp_i64_rev(const void *b, const void *a) { COMP_FUNC_BODY(i64) }
static int cmp_u8_rev(const void *b, const void *a) { COMP_FUNC_BODY(u8) }
static int cmp_u16_rev(const void *b, const void *a) { COMP_FUNC_BODY(u16) }
static int cmp_u32_rev(const void *b, const void *a) { COMP_FUNC_BODY(u32) }
static int cmp_u64_rev(const void *b, const void *a) { COMP_FUNC_BODY(u64) }
static int cmp_float_rev(const void *b, const void *a) { COMP_FUNC_BODY(float) }
static int cmp_double_rev(const void *b, const void *a) { COMP_FUNC_BODY(double) }
#undef COMP_FUNC_BODY

// Jump table initialization
static SetterFunc setters[VTSF64+1] = {
	[VTSI08] = set_i8,
	[VTSI16] = set_i16,
	[VTSI32] = set_i32,
	[VTSI64] = set_i64,
	[VTUI08] = set_u8,
	[VTUI16] = set_u16,
	[VTUI32] = set_u32,
	[VTUI64] = set_u64,
	[VTSF32] = set_float,
	[VTSF64] = set_double
};
static GetterFunc getters[VTSF64 + 1] = {
	[VTSI08] = get_i8,
	[VTSI16] = get_i16,
	[VTSI32] = get_i32,
	[VTSI64] = get_i64,
	[VTUI08] = get_u8,
	[VTUI16] = get_u16,
	[VTUI32] = get_u32,
	[VTUI64] = get_u64,
	[VTSF32] = get_float,
	[VTSF64] = get_double
};

static CompareFunc compares[VTSF64 + 1] = {
	[VTSI08] = cmp_i8,
	[VTSI16] = cmp_i16,
	[VTSI32] = cmp_i32,
	[VTSI64] = cmp_i64,
	[VTUI08] = cmp_u8,
	[VTUI16] = cmp_u16,
	[VTUI32] = cmp_u32,
	[VTUI64] = cmp_u64,
	[VTSF32] = cmp_float,
	[VTSF64] = cmp_double
};
static CompareFunc compares_rev[VTSF64 + 1] = {
	[VTSI08] = cmp_i8_rev,
	[VTSI16] = cmp_i16_rev,
	[VTSI32] = cmp_i32_rev,
	[VTSI64] = cmp_i64_rev,
	[VTUI08] = cmp_u8_rev,
	[VTUI16] = cmp_u16_rev,
	[VTUI32] = cmp_u32_rev,
	[VTUI64] = cmp_u64_rev,
	[VTSF32] = cmp_float_rev,
	[VTSF64] = cmp_double_rev
};

FORCE_INLINE
void get_vect(REBCNT type, REBYTE *data, REBCNT n, REBVAL *val) {
	ASSERT1(type <= VTSF64, RP_BAD_SIZE);
	getters[type](data, n, val);
}

FORCE_INLINE
REBDEC get_vect_decimal(REBCNT type, REBYTE *data, REBCNT n) {
	ASSERT1(type <= VTSF64, RP_BAD_SIZE);
	REBVAL val;
	getters[type](data, n, &val);
	if (type >= VTSF08) return VAL_DECIMAL(&val);
	if (type <= VTUI64) return (REBDEC)VAL_INT64(&val);
	return (REBDEC)VAL_UNT64(&val);
}

FORCE_INLINE
void set_vect(REBCNT type, REBYTE *data, REBCNT n, REBVAL *val) {
	ASSERT1(type <= VTSF64, RP_BAD_SIZE);
	setters[type](data, n, val);
}


// Query functions
typedef struct Vector_Query_Values {
	REBLEN length;
	REBDEC minimum;
	REBDEC maximum;
	REBDEC sum;
	REBDEC mean;
	REBDEC variance;
	REBDEC median;
} REBVQV;

static void Query_Vector_Statictics(REBSER *vect, REBVQV *out) {
	REBLEN len = SERIES_TAIL(vect);
	REBCNT type = VECT_TYPE(vect);
	REBCNT n = 0;
	REBYTE *data = SERIES_DATA(vect);
	REBDEC num, diff;

	CLEARS(out);
	if (len == 0) return;
	out->length = len;
	out->minimum = get_vect_decimal(type, data, 0);
	out->maximum = out->minimum;
	for (; n < len; n++) {
		num = get_vect_decimal(type, data, n);
		// Min/Max
		if (num < out->minimum) out->minimum = num;
		else if (num > out->maximum) out->maximum = num;
		// Sum
		out->sum += num;
	}
	// Mean
	out->mean = out->sum / len;
	// Calculate squared differences and variance
	for (n = 0; n < len; n++) {
		num = get_vect_decimal(type, data, n);
		diff = num - out->mean;
		out->variance += diff * diff;  // More efficient than pow()
	}
}
static REBDEC Query_Vector_Median(REBSER *vect) {
	REBLEN len = SERIES_TAIL(vect);
	REBCNT type = VECT_TYPE(vect);
	REBSER *sorted;
	REBDEC median;

	if (len == 0) return 0;
	// Make a vector copy, because sorting modifies
	sorted = Copy_Series(vect);
	sorted->size = vect->size; // attributes
	reb_qsort(SERIES_DATA(sorted), len, VECT_BYTE_SIZE(type), compares[type]);

	median = get_vect_decimal(type, SERIES_DATA(sorted), len/2);
	if (len%2 == 0) {
		// Even number of elements
		median = (get_vect_decimal(type, SERIES_DATA(sorted), len/2-1) + median) / 2.0;
	}
	Free_Series(sorted);
	return median;
}


FORCE_INLINE
void Set_Vector_Value(REBCNT bits, REBYTE *data, REBCNT n, REBVAL *val) {
	REBVAL num = *val; // because may be modified!
	if (IS_DECIMAL(val)) {
		// value is decimal
		if (bits <= VTUI64) {
			// but target is integer 
			VAL_INT64(&num) = (REBI64)VAL_DECIMAL(val);
		}
	}
	else if (IS_INTEGER(val) || IS_CHAR(val)) {
		if (bits > VTUI64) {
			VAL_DECIMAL(&num) = (REBDEC)VAL_INT64(val);
		}
	}
	else Trap_Arg(val);
	setters[bits](data, n, &num);
}


void Set_Vector_Row(REBSER *ser, REBVAL *blk)
{
	REBVAL *val;
	REBLEN n = 0;
	REBCNT len = VAL_LEN(blk);
	REBCNT bits = VECT_TYPE(ser);

	if (IS_BLOCK(blk)) {
		val = VAL_BLK_DATA(blk);
		for (; NOT_END(val); val++) {
			Set_Vector_Value(bits, ser->data, n++, val);
		}
	}
	else {
#ifdef old_code
		REBYTE *data = VAL_BIN_DATA(blk);
		for (; len > 0; len--, idx++) {
			set_vect(bits, ser->data, n++, (REBI64)(data[idx]), f);
		}
#else
		REBCNT bytes = ser->tail * SERIES_WIDE(ser); //TODO: review! Wide is max 256 bytes!!!
		if (len > bytes) len = bytes;
		COPY_MEM(ser->data, VAL_BIN_DATA(blk), len);
#endif
	}
}

void Find_Minimum_Of_Vector(REBSER *vect, REBVAL *ret) {
	REBLEN len;
	REBYTE *data;
	
	len = SERIES_TAIL(vect);

	SET_NONE(ret);
	if (len == 0) return;

#define FIND_MIN_INT(type) {             \
        type *typed_data = (type *)data;     \
        type min_value = typed_data[0];      \
        for (REBLEN i = 1; i < len; i++) {   \
            if (typed_data[i] < min_value)   \
                min_value = typed_data[i];   \
        }                                    \
        SET_INTEGER(ret, min_value);         \
        return;                              \
    }

#define FIND_MIN_DEC(type) {             \
        type *typed_data = (type *)data;     \
        type min_value = typed_data[0];      \
        for (REBLEN i = 1; i < len; i++) {   \
            if (typed_data[i] < min_value)   \
                min_value = typed_data[i];   \
        }                                    \
        SET_DECIMAL(ret, min_value);         \
        return;                              \
    }

	data = SERIES_DATA(vect);

	switch (VECT_TYPE(vect)) {
	case VTSI08: FIND_MIN_INT(i8); break;
	case VTSI16: FIND_MIN_INT(i16); break;
	case VTSI32: FIND_MIN_INT(i32); break;
	case VTSI64: FIND_MIN_INT(i64); break;
	case VTUI08: FIND_MIN_INT(u8); break;
	case VTUI16: FIND_MIN_INT(u16); break;
	case VTUI32: FIND_MIN_INT(u32); break;
	case VTUI64: FIND_MIN_INT(u64); break;
	case VTSF32: FIND_MIN_DEC(float); break;
	case VTSF64: FIND_MIN_DEC(double); break;
	}

#undef FIND_MIN_INT
#undef FIND_MIN_DEC
}

void Find_Maximum_Of_Vector(REBSER *vect, REBVAL *ret) {
	REBLEN len;
	REBYTE *data;

	len = SERIES_TAIL(vect);

	SET_NONE(ret);
	if (len == 0) return;

#define FIND_MAX_INT(type) {             \
        type *typed_data = (type *)data;     \
        type max_value = typed_data[0];      \
        for (REBLEN i = 1; i < len; i++) {   \
            if (typed_data[i] > max_value)   \
                max_value = typed_data[i];   \
        }                                    \
        SET_INTEGER(ret, max_value);         \
        return;                              \
    }

#define FIND_MAX_DEC(type) {             \
        type *typed_data = (type *)data;     \
        type max_value = typed_data[0];      \
        for (REBLEN i = 1; i < len; i++) {   \
            if (typed_data[i] > max_value)   \
                max_value = typed_data[i];   \
        }                                    \
        SET_DECIMAL(ret, max_value);         \
        return;                              \
    }

	data = SERIES_DATA(vect);

	switch (VECT_TYPE(vect)) {
	case VTSI08: FIND_MAX_INT(i8); break;
	case VTSI16: FIND_MAX_INT(i16); break;
	case VTSI32: FIND_MAX_INT(i32); break;
	case VTSI64: FIND_MAX_INT(i64); break;
	case VTUI08: FIND_MAX_INT(u8); break;
	case VTUI16: FIND_MAX_INT(u16); break;
	case VTUI32: FIND_MAX_INT(u32); break;
	case VTUI64: FIND_MAX_INT(u64); break;
	case VTSF32: FIND_MAX_DEC(float); break;
	case VTSF64: FIND_MAX_DEC(double); break;
	}

#undef FIND_MAX_INT
#undef FIND_MAX_DEC
}


/***********************************************************************
**
*/	static REBOOL Query_Vector_Field(REBSER *vect, REBCNT field, REBVAL *ret, REBVQV *vqv)
/*
**		Set a value with requested vector field result 
**
***********************************************************************/
{
#define RETURN_DECIMAL(v) {SET_DECIMAL(ret, v); return TRUE;}
#define RETURN_NUMBER(v)  {SET_DECIMAL(ret, v); goto return_number;}

	switch (field) {
	case SYM_TYPE:
		Init_Word(ret, (VECT_TYPE(vect) >= VTSF08) ? SYM_DECIMAL_TYPE : SYM_INTEGER_TYPE);
		break;
	case SYM_SIZE:
		SET_INTEGER(ret, VECT_BIT_SIZE(VECT_TYPE(vect)));
		break;
	case SYM_LENGTH:
		SET_INTEGER(ret, vect->tail);
		break;
	case SYM_SIGNED:
		SET_LOGIC(ret, !(VECT_TYPE(vect) >= VTUI08 && VECT_TYPE(vect) <= VTUI64));
		break;
	case SYM_MIN:
	case SYM_MINIMUM:
		if (vqv) RETURN_NUMBER(vqv->minimum);
		Find_Minimum_Of_Vector(vect, ret);
		break;
	case SYM_MAX:
	case SYM_MAXIMUM:
		if (vqv) RETURN_NUMBER(vqv->maximum);
		Find_Maximum_Of_Vector(vect, ret);
		break;
	default:
		if (!vqv) {
			REBVQV out;
			Query_Vector_Statictics(vect, &out);
			vqv = &out;
		}
		if (field == SYM_RANGE) RETURN_NUMBER((vqv->maximum - vqv->minimum));
		if (field == SYM_SUM) RETURN_NUMBER(vqv->sum);
		if (field == SYM_MEAN || field == SYM_AVERAGE) RETURN_DECIMAL(vqv->mean);
		if (field == SYM_MEDIAN) RETURN_DECIMAL(Query_Vector_Median(vect));
		if (field == SYM_VARIANCE) RETURN_DECIMAL(vqv->variance);
		if (field == SYM_POPULATION_DEVIATION) RETURN_DECIMAL(sqrt(vqv->variance / SERIES_TAIL(vect)));
		if (field == SYM_SAMPLE_DEVIATION) RETURN_DECIMAL(sqrt(vqv->variance / (SERIES_TAIL(vect) - 1)));
		return FALSE;
	}
	return TRUE;
return_number:
	// Return integer if vector type is integer, else keep decimal
	if (VECT_TYPE(vect) < VTSF08) SET_INTEGER(ret, (REBI64)VAL_DECIMAL(ret));
	return TRUE;

#undef RETURN_DECIMAL
#undef RETURN_NUMBER
}


/***********************************************************************
**
*/	REBSER *Make_Vector_Block(REBVAL *vect)
/*
**		Convert a vector to a block.
**
***********************************************************************/
{
	REBCNT len = VAL_LEN(vect);
	REBYTE *data = VAL_SERIES(vect)->data;
	REBCNT type = VECT_TYPE(VAL_SERIES(vect));
	REBSER *ser = Make_Block(len);
	REBVAL *val = NULL;
	REBCNT reb_type = (type >= VTSF08) ? REB_DECIMAL : REB_INTEGER;

	if (len > 0) {
		val = BLK_HEAD(ser);
		for (REBCNT n = VAL_INDEX(vect); n < VAL_TAIL(vect); n++, val++) {
			VAL_SET(val, reb_type);
			get_vect(type, data, n, val);
		}
		SET_END(val);
	}
	SERIES_TAIL(ser) = len;
	return ser;
}

#ifndef EXCLUDE_VECTOR_MATH
/***********************************************************************
**
*/	REBVAL* Math_Op_Vector(REBVAL *v1, REBVAL *v2, REBCNT action)
/*
**		Do basic math operation on a vector
**
***********************************************************************/
{
	REBSER *vect = NULL;
	REBYTE *data;
	REBCNT bits;
	REBCNT len;

	REBVAL *left;
	REBVAL *right;

	REBI64 i = 0;
	REBDEC f = 0;
	REBCNT n = 0;

	if (IS_VECTOR(v1) && IS_NUMBER(v2)) {
		left = v1;
		right = v2;
	} else if (IS_VECTOR(v2) && IS_NUMBER(v1)) {
		left = v2;
		right = v1;
	} else {
		Trap_Action(VAL_TYPE(v1), action);
		return NULL;
	}
	vect = VAL_SERIES(left);
	bits = VECT_TYPE(vect);
	data = vect->data;

	if (IS_INTEGER(right)) {
		i = VAL_INT64(right);
		f = (REBDEC)i;
	} else {
		f = VAL_DECIMAL(right);
		i = (REBI64)f;
	}

	n = VAL_INDEX(left);
	len = n + VAL_LEN(left);

	switch (action) {
		case A_ADD:
			switch (bits) {
			case VTSI08: for (; n<len; n++) ( (i8*)data)[n] += ( i8)i; break;
			case VTSI16: for (; n<len; n++) ((i16*)data)[n] += (i16)i; break;
			case VTSI32: for (; n<len; n++) ((i32*)data)[n] += (i32)i; break;
			case VTSI64: for (; n<len; n++) ((i64*)data)[n] += (i64)i; break;
			case VTUI08: for (; n<len; n++) (( u8*)data)[n] += ( u8)i; break;
			case VTUI16: for (; n<len; n++)	((u16*)data)[n] += (u16)i; break;
			case VTUI32: for (; n<len; n++) ((u32*)data)[n] += (u32)i; break;
			case VTUI64: for (; n<len; n++)	((i64*)data)[n] += (u64)i; break;
//			case VTSF08:
//			case VTSF16:
			case VTSF32: for (; n<len; n++) (( float*)data)[n] += (float)f; break;
			case VTSF64: for (; n<len; n++) ((double*)data)[n] += f; break;
			}
			break;
		case A_SUBTRACT:
			switch (bits) {
			case VTSI08: for (; n<len; n++) (( i8*)data)[n] -= ( i8)i; break;
			case VTSI16: for (; n<len; n++) ((i16*)data)[n] -= (i16)i; break;
			case VTSI32: for (; n<len; n++) ((i32*)data)[n] -= (i32)i; break;
			case VTSI64: for (; n<len; n++) ((i64*)data)[n] -= (i64)i; break;
			case VTUI08: for (; n<len; n++) (( u8*)data)[n] -= ( u8)i; break;
			case VTUI16: for (; n<len; n++)	((u16*)data)[n] -= (u16)i; break;
			case VTUI32: for (; n<len; n++) ((u32*)data)[n] -= (u32)i; break;
			case VTUI64: for (; n<len; n++)	((i64*)data)[n] -= (u64)i; break;
//			case VTSF08:
//			case VTSF16:
			case VTSF32: for (; n<len; n++) (( float*)data)[n] -= (float)f; break;
			case VTSF64: for (; n<len; n++) ((double*)data)[n] -= f; break;
			}
			break;
		case A_MULTIPLY:
			switch (bits) {
			case VTSI08: for (; n<len; n++) (( i8*)data)[n] *= ( i8)i; break;
			case VTSI16: for (; n<len; n++) ((i16*)data)[n] *= (i16)i; break;
			case VTSI32: for (; n<len; n++) ((i32*)data)[n] *= (i32)i; break;
			case VTSI64: for (; n<len; n++) ((i64*)data)[n] *= (i64)i; break;
			case VTUI08: for (; n<len; n++) (( u8*)data)[n] *= ( u8)i; break;
			case VTUI16: for (; n<len; n++)	((u16*)data)[n] *= (u16)i; break;
			case VTUI32: for (; n<len; n++) ((u32*)data)[n] *= (u32)i; break;
			case VTUI64: for (; n<len; n++)	((i64*)data)[n] *= (u64)i; break;
//			case VTSF08:
//			case VTSF16:
			case VTSF32: for (; n<len; n++) (( float*)data)[n] *= (float)f; break;
			case VTSF64: for (; n<len; n++) ((double*)data)[n] *= f; break;
			}
			break;
		case A_DIVIDE:
			if (i == 0) Trap0(RE_ZERO_DIVIDE);
			switch (bits) {
			case VTSI08: for (; n<len; n++) (( i8*)data)[n] /= ( i8)i; break;
			case VTSI16: for (; n<len; n++) ((i16*)data)[n] /= (i16)i; break;
			case VTSI32: for (; n<len; n++) ((i32*)data)[n] /= (i32)i; break;
			case VTSI64: for (; n<len; n++) ((i64*)data)[n] /= (i64)i; break;
			case VTUI08: for (; n<len; n++) (( u8*)data)[n] /= ( u8)i; break;
			case VTUI16: for (; n<len; n++)	((u16*)data)[n] /= (u16)i; break;
			case VTUI32: for (; n<len; n++) ((u32*)data)[n] /= (u32)i; break;
			case VTUI64: for (; n<len; n++)	((i64*)data)[n] /= (u64)i; break;
//			case VTSF08:
//			case VTSF16:
			case VTSF32: for (; n<len; n++) (( float*)data)[n] /= (float)f; break;
			case VTSF64: for (; n<len; n++) ((double*)data)[n] /= f; break;
			}
			break;
	}
	return left;
}
#endif

/***********************************************************************
**
*/	REBINT Compare_Vector(REBVAL *a, REBVAL *b)
/*
***********************************************************************/
{
	REBCNT l1 = VAL_LEN(a);
	REBCNT l2 = VAL_LEN(b);
	REBCNT len = MIN(l1, l2);
	REBCNT n;
	REBVAL v1;
	REBVAL v2;
	REBYTE *d1 = VAL_SERIES(a)->data;
	REBYTE *d2 = VAL_SERIES(b)->data;
	REBCNT b1 = VECT_TYPE(VAL_SERIES(a));
	REBCNT b2 = VECT_TYPE(VAL_SERIES(b));

	if (
		(b1 >= VTSF08 && b2 < VTSF08)
		|| (b2 >= VTSF08 && b1 < VTSF08)
	) Trap0(RE_NOT_SAME_TYPE);

	for (n = 0; n < len; n++) {
		get_vect(b1, d1, n + VAL_INDEX(a), &v1);
		get_vect(b2, d2, n + VAL_INDEX(b), &v2);
		if (VAL_UNT64(&v1) != VAL_UNT64(&v2)) break;
	}

	if (n != len) {
		if (VAL_UNT64(&v1) > VAL_UNT64(&v2)) return 1;
		return -1;
	}

	return l1 - l2;
}


/***********************************************************************
**
*/	void Shuffle_Vector(REBVAL *vect, REBFLG secure)
/*
***********************************************************************/
{
	REBCNT n;
	REBCNT k;
	REBVAL a, b;
	REBYTE *data = VAL_SERIES(vect)->data;
	REBCNT type = VECT_TYPE(VAL_SERIES(vect));
	REBCNT idx = VAL_INDEX(vect);

	for (n = VAL_LEN(vect); n > 1;) {
		k = idx + (REBCNT)Random_Int(secure) % n;
		n--;
		get_vect(type, data, k, &a);
		get_vect(type, data, n + idx, &b);
		set_vect(type, data, k, &b);
		set_vect(type, data, n + idx, &a);
	}
}

/***********************************************************************
**
*/	void Sort_Vector(REBVAL *vect, REBLEN len, REBFLG reversed)
/*
***********************************************************************/
{
	REBCNT type = VECT_TYPE(VAL_SERIES(vect));
	REBCNT idx = VAL_INDEX(vect);
	REBCNT skp = VECT_BYTE_SIZE(type);
	REBYTE *data = VAL_SERIES(vect)->data + (idx * skp);

	reb_qsort(data, len, skp, reversed ? compares_rev[type] : compares[type]);
}

/***********************************************************************
**
*/	void Get_Vector_Value(REBVAL *var, REBSER *series, REBCNT index)
/*
***********************************************************************/
{
	REBYTE *data = series->data;
	REBCNT bits = VECT_TYPE(series);

	get_vect(bits, data, index, var);
	SET_TYPE(var, (bits >= VTSF08) ? REB_DECIMAL : REB_INTEGER);
}


/***********************************************************************
**
*/	REBSER *Make_Vector(REBINT type, REBINT sign, REBINT dims, REBINT bits, REBINT size)
/*
**		type: the datatype
**		sign: signed or unsigned
**		dims: number of dimensions
**		bits: number of bits per unit (8, 16, 32, 64)
**		size: number of values
**
***********************************************************************/
{
	REBCNT len;
	REBSER *ser;

	//printf("MAKE_VECTOR=> type: %i sign: %i dims: %i bits: %i size: %i\n", type, sign, dims, bits, size);

	len = size * dims;
	if (len > 0x7fffffff) return 0;
	ser = Make_Series(len+1, bits/8, TRUE); // !!! can width help extend the len?
	LABEL_SERIES(ser, "make vector");
	//No need to clear the series, because Make_Series guarantees completely cleared memory.
	ser->tail = len;  // !!! another way to do it?

	// Store info about the vector (could be moved to flags if necessary):
	ser->size = (dims << 8) | (type << 3) | (sign << 2) | (bits == 64 ? 3 : bits >> 4); // there are only 2 bits to store the info

	return ser;
}

REBOOL Get_Vector_Spec_From_Symbol(REBCNT sym, REBINT *type, REBINT *sign, REBINT *bits) {
	switch (Normalize_Vector_Type_Symbol(sym)) {
	case SYM_INT8X:    *type = 0; *sign = 0; *bits =  8; break;
	case SYM_UINT8X:   *type = 0; *sign = 1; *bits =  8; break;
	case SYM_INT16X:   *type = 0; *sign = 0; *bits = 16; break;
	case SYM_UINT16X:  *type = 0; *sign = 1; *bits = 16; break;
	case SYM_INT32X:   *type = 0; *sign = 0; *bits = 32; break;
	case SYM_UINT32X:  *type = 0; *sign = 1; *bits = 32; break;
	case SYM_INT64X:   *type = 0; *sign = 0; *bits = 64; break;
	case SYM_UINT64X:  *type = 0; *sign = 1; *bits = 64; break;
	case SYM_FLOAT32X: *type = 1; *sign = 0; *bits = 32; break;
	case SYM_FLOAT64X: *type = 1; *sign = 0; *bits = 64; break;
	default: return FALSE;
	}
	return TRUE;
}

/***********************************************************************
**
*/	REBSER *Make_Vector_From_Word(REBCNT sym, REBINT size)
/*
**	Make a vector from a type name.
**
***********************************************************************/
{
	REBINT type, sign, bits;
	if (Get_Vector_Spec_From_Symbol(sym, &type, &sign, &bits)) {
		return Make_Vector(type, sign, 1, bits, size);
	}
	return NULL;	
}

/***********************************************************************
**
*/	REBVAL *Make_Vector_Spec(REBVAL *bp, REBVAL *value)
/*
**	Make a vector from a block spec.
**
**     make vector! [integer! 32 100]
**     make vector! [decimal! 64 100]
**     make vector! [unsigned integer! 32]
**     Fields:
**          signed:     signed, unsigned
**    		datatypes:  integer, decimal
**    		dimensions: 1 - N
**    		bitsize:    1, 8, 16, 32, 64
**    		size:       integer units
**    		init:		block of values
**
***********************************************************************/
{
	REBINT type = -1; // 0 = int,    1 = float
	REBINT sign = -1; // 0 = signed, 1 = unsigned
	REBINT dims = 1;
	REBINT bits = 32;
	REBCNT size = 0;
	REBSER *vect;
	REBVAL *iblk = 0;

	// SIGNED / UNSIGNED
	if (IS_WORD(bp)) {
		if (Get_Vector_Spec_From_Symbol(VAL_WORD_CANON(bp), &type, &sign, &bits)) {
			bp++;
			goto size_spec;
		}
		switch (VAL_WORD_CANON(bp)) {
		case SYM_UNSIGNED: sign = 1; bp++; break;
		case SYM_SIGNED:   sign = 0; bp++; break;
		}
	}

	// INTEGER! or DECIMAL!
	if (IS_WORD(bp)) {
		if (VAL_WORD_CANON(bp) == (REB_INTEGER+1)) // integer! symbol
			type = 0;
		else if (VAL_WORD_CANON(bp) == (REB_DECIMAL+1)) { // decimal! symbol
			type = 1;
			if (sign > 0) return 0;
		}
		else return 0;
		bp++;
	}

	if (type < 0) type = 0;
	if (sign < 0) sign = 0;

	// BITS
	if (IS_INTEGER(bp)) {
		bits = Int32(bp);
		if (
			(bits == 32 || bits == 64)
			||
			(type == 0 && (bits == 8 || bits == 16))
		) bp++;
		else return 0;
	} else return 0;

size_spec:

	// SIZE
	if (IS_INTEGER(bp)) {
		size = Int32(bp);
		if (size < 0) return 0;
		bp++;
	}

	// Initial data:
	if (IS_BLOCK(bp) || IS_BINARY(bp)) {
		REBCNT len = VAL_LEN(bp);
		if (IS_BINARY(bp)) len /= (bits >> 3);
		if (len > size && size == 0) size = len;
		iblk = bp;
		bp++;
	}

	// Index offset:
	if (IS_INTEGER(bp)) {
		VAL_INDEX(value) = (Int32s(bp, 1) - 1);
		bp++;
	}
	else VAL_INDEX(value) = 0;

	if (NOT_END(bp)) return 0;

	vect = Make_Vector(type, sign, dims, bits, size);
	if (!vect) return 0;

	if (iblk) Set_Vector_Row(vect, iblk);

	SET_TYPE(value, REB_VECTOR);
	VAL_SERIES(value) = vect;
	// index set earlier

	return value;
}


/***********************************************************************
**
*/	REBFLG MT_Vector(REBVAL *out, REBVAL *data, REBCNT type)
/*
***********************************************************************/
{
	if (Make_Vector_Spec(data, out)) return TRUE;
	return FALSE;
}


/***********************************************************************
**
*/	REBINT CT_Vector(REBVAL *a, REBVAL *b, REBINT mode)
/*
***********************************************************************/
{
	REBINT num;

	if (mode == 3)
		return VAL_SERIES(a) == VAL_SERIES(b) && VAL_INDEX(a) == VAL_INDEX(b);

	num = Compare_Vector(a, b);
	if (mode >= 0) return (num == 0);
	if (mode == -1) return (num >= 0);
	return (num > 0);
}


/***********************************************************************
**
*/	REBINT PD_Vector(REBPVS *pvs)
/*
***********************************************************************/
{
	REBVAL *sel = pvs->select;
	REBVAL *val = pvs->value;
	REBVAL *set = pvs->setval;
	REBSER *vect = VAL_SERIES(val);
	REBINT bits = VECT_TYPE(vect);
	REBINT n;
	//REBINT dims;
	
	REBYTE *vp;
	REBI64 i = 0;
	REBDEC f = 0.0;

	if (IS_INTEGER(sel) || IS_DECIMAL(sel)) {
		n = Int32(sel);
		if (n == 0) return (pvs->setval) ? PE_BAD_RANGE : PE_NONE; // allow PICK with zero index but not for POKE
		if (n < 0) n++;
	} else if (IS_WORD(sel)) {
		if (set == 0) {
			val = pvs->value = pvs->store;
			if(!Query_Vector_Field(vect, VAL_WORD_CANON(sel), val, NULL)) return PE_BAD_SELECT;
			return PE_OK;
		} else
			return PE_BAD_SET;
	} else  return PE_BAD_SELECT;

	n += VAL_INDEX(val);
	vect = VAL_SERIES(val);
	vp   = vect->data;
	
	//dims = vect->size >> 8;

	if (pvs->setval == 0) {

		// Check range:
		if (n <= 0 || (REBCNT)n > vect->tail) return PE_NONE;

		// Get element value:
		get_vect(bits, vp, n - 1, pvs->store);
		SET_TYPE(pvs->store, (bits >= VTSF08) ? REB_DECIMAL : REB_INTEGER);
		return PE_USE;
	}

	//--- Set Value...
	TRAP_PROTECT(vect);

	if (n <= 0 || (REBCNT)n > vect->tail) return PE_BAD_RANGE;
	Set_Vector_Value(bits, vp, n-1, set);
	return PE_OK;
}


static void reverse_vector(REBVAL *value, REBCNT len)
{
	REBCNT n;
	REBCNT m;
	REBINT width = VAL_VEC_WIDTH(value);

	if (width == 1) {
		REBYTE *bp = VAL_BIN_DATA(value);
		REBYTE c1;
		for (n = 0, m = len-1; n < len / 2; n++, m--) {
			c1 = bp[n];
			bp[n] = bp[m];
			bp[m] = c1;
		}
	}
	else if (width == 2) {
		REBUNI *up = VAL_UNI_DATA(value);
		REBUNI c2;
		for (n = 0, m = len-1; n < len / 2; n++, m--) {
			c2 = up[n];
			up[n] = up[m];
			up[m] = c2;
		}
	}
	else if (width == 4) {
		REBCNT *i4 = (REBCNT*)VAL_DATA(value);
		REBCNT c4;
		for (n = 0, m = len-1; n < len / 2; n++, m--) {
			c4 = i4[n];
			i4[n] = i4[m];
			i4[m] = c4;
		}
	}
	else if (width == 8) {
		REBU64 *i8 = (REBU64*)VAL_DATA(value);
		REBU64 c8;
		for (n = 0, m = len-1; n < len / 2; n++, m--) {
			c8 = i8[n];
			i8[n] = i8[m];
			i8[m] = c8;
		}
	}
}


/***********************************************************************
**
*/	REBTYPE(Vector)
/*
***********************************************************************/
{
	REBVAL *value = D_ARG(1);
	REBVAL *arg = D_ARG(2);
	REBINT type;
	REBCNT size, bits;
	REBSER *vect;
	REBSER *ser;
	REBSER *blk;
	REBVAL *val;
	REBINT	len;

	type = Do_Series_Action(action, value, arg);
	if (type >= 0) return type;

	vect = VAL_SERIES(value); // not valid for MAKE or TO

	// Check must be in this order (to avoid checking a non-series value);
	if (action >= A_TAKE && action <= A_SORT && IS_PROTECT_SERIES(vect))
		Trap0(RE_PROTECTED);

	switch (action) {

	case A_PICK:
		Pick_Path(value, arg, 0);
		return R_TOS;

	case A_POKE:
		Pick_Path(value, arg, D_ARG(3));
		return R_ARG3;

#ifndef EXCLUDE_VECTOR_MATH
	case A_ADD:
	case A_SUBTRACT:
	case A_MULTIPLY:
	case A_DIVIDE:
		Math_Op_Vector(value, arg, action);
		break;
#endif

	case A_MAKE:
		// We only allow MAKE VECTOR! ...
		if (!IS_DATATYPE(value)) goto bad_make;

		// CASE: make vector! 100
		if (IS_INTEGER(arg) || IS_DECIMAL(arg)) {
			size = Int32s(arg, 0);
			if (size < 0) goto bad_make;
			ser = Make_Vector(0, 0, 1, 32, size);
			SET_VECTOR(value, ser);
			break;
		}
//		if (IS_NONE(arg)) {
//			ser = Make_Vector(0, 0, 1, 32, 0);
//			SET_VECTOR(value, ser);
//			break;
//		}
		// fall thru

	case A_TO:
		// CASE: make vector! [...]
		if (IS_BLOCK(arg) && Make_Vector_Spec(VAL_BLK_DATA(arg), value)) break;
		goto bad_make;

	case A_LENGTHQ:
		//bits = 1 << (vect->size & 3);
		SET_INTEGER(D_RET, vect->tail);
		return R_RET;

	case A_COPY:
		len = Partial(value, 0, D_ARG(3), 0); // Can modify value index.
		ser = Copy_Series_Part(vect, VAL_INDEX(value), len);
		ser->size = vect->size; // attributes
		SET_VECTOR(value, ser);
		break;

	case A_REVERSE:
		len = Partial(value, 0, D_ARG(3), 0);
		if (len > 0) reverse_vector(value, len);
		break;

	case A_SORT:
		len = Partial(value, 0, D_ARG(8), 0);
		Sort_Vector(value, len, D_REF(10));
		if (
		//	D_REF(2) ||	// case sensitive
			D_REF(3) ||	// skip
			D_REF(5) 	// comparator
		//	D_REF(9) 	// all fields
			) Trap0(RE_FEATURE_NA);
		break;
			
	case A_RANDOM:
		if (D_REF(2) || D_REF(4)) Trap0(RE_BAD_REFINES); // /seed /only
		Shuffle_Vector(value, D_REF(3));
		return R_ARG1;

	case A_REFLECT:
		bits = VECT_TYPE(vect);
		if (SYM_SPEC == VAL_WORD_SYM(D_ARG(2))) {
			blk = Make_Block(4);
			if (bits >= VTUI08 && bits <= VTUI64) Init_Word(Append_Value(blk), SYM_UNSIGNED);
			Query_Vector_Field(vect, SYM_TYPE, Append_Value(blk), NULL);
			Query_Vector_Field(vect, SYM_SIZE, Append_Value(blk), NULL);
			Query_Vector_Field(vect, SYM_LENGTH, Append_Value(blk), NULL);
			Set_Series(REB_BLOCK, value, blk);
		} else {
			if(!Query_Vector_Field(vect, VAL_WORD_SYM(D_ARG(2)), value, NULL))
				Trap_Reflect(VAL_TYPE(value), D_ARG(2));
		}
		break;

	case A_QUERY:
		bits = VECT_TYPE(vect);
		REBVAL *spec = Get_System(SYS_STANDARD, STD_VECTOR_INFO);
		if (!IS_OBJECT(spec)) Trap_Arg(spec);
		REBVAL *field = D_ARG(ARG_QUERY_FIELD);
		if(IS_WORD(field)) {
			if (!Query_Vector_Field(vect, VAL_WORD_SYM(field), value, NULL))
				Trap_Reflect(VAL_TYPE(value), field); // better error?
			break;
		}
		REBVQV results = { 0 };
		Query_Vector_Statictics(vect, &results);

		if (IS_BLOCK(field)) {
			REBSER *values = Make_Block(2 * BLK_LEN(VAL_SERIES(field)));
			REBVAL *word = VAL_BLK_DATA(field);
			for (; NOT_END(word); word++) {
				if (ANY_WORD(word)) {
					if (!IS_GET_WORD(word)) {
						// keep the word as a key (converted to the set-word) in the result
						val = Append_Value(values);
						*val = *word;
						VAL_TYPE(val) = REB_SET_WORD;
						VAL_SET_LINE(val);
					}
					val = Append_Value(values);
					if (!Query_Vector_Field(vect, VAL_WORD_SYM(word), val, &results))
						Trap1(RE_INVALID_ARG, word);
				}
				else  Trap1(RE_INVALID_ARG, word);
			}
			Set_Series(REB_BLOCK, value, values);
		}
		else if (IS_NONE(field)) {
			Set_Block(D_RET, Get_Object_Words(spec));
			return R_RET;
		}
		else {
			REBSER *obj = CLONE_OBJECT(VAL_OBJ_FRAME(spec));
			Query_Vector_Field(vect, SYM_SIGNED, OFV(obj, STD_VECTOR_INFO_SIGNED), &results);
			Query_Vector_Field(vect, SYM_TYPE,   OFV(obj, STD_VECTOR_INFO_TYPE), &results);
			Query_Vector_Field(vect, SYM_SIZE,   OFV(obj, STD_VECTOR_INFO_SIZE), &results);
			Query_Vector_Field(vect, SYM_LENGTH, OFV(obj, STD_VECTOR_INFO_LENGTH), &results);
			Query_Vector_Field(vect, SYM_MINIMUM, OFV(obj, STD_VECTOR_INFO_MINIMUM), &results);
			Query_Vector_Field(vect, SYM_MAXIMUM, OFV(obj, STD_VECTOR_INFO_MAXIMUM), &results);
			Query_Vector_Field(vect, SYM_RANGE, OFV(obj, STD_VECTOR_INFO_RANGE), &results);
			Query_Vector_Field(vect, SYM_SUM, OFV(obj, STD_VECTOR_INFO_SUM), &results);
			Query_Vector_Field(vect, SYM_MEAN, OFV(obj, STD_VECTOR_INFO_MEAN), &results);
			Query_Vector_Field(vect, SYM_MEDIAN, OFV(obj, STD_VECTOR_INFO_MEDIAN), &results);
			Query_Vector_Field(vect, SYM_VARIANCE, OFV(obj, STD_VECTOR_INFO_VARIANCE), &results);
			Query_Vector_Field(vect, SYM_POPULATION_DEVIATION, OFV(obj, STD_VECTOR_INFO_POPULATION_DEVIATION), &results);
			Query_Vector_Field(vect, SYM_SAMPLE_DEVIATION, OFV(obj, STD_VECTOR_INFO_SAMPLE_DEVIATION), &results);
			SET_OBJECT(value, obj);
		}
		break;

	default:
		Trap_Action(VAL_TYPE(value), action);
	}

	*D_RET = *value;
	return R_RET;

bad_make:
	Trap_Make(REB_VECTOR, arg);
}


/***********************************************************************
**
*/	void Mold_Vector(REBVAL *value, REB_MOLD *mold, REBFLG molded)
/*
***********************************************************************/
{
	REBSER *vect = VAL_SERIES(value);
	REBYTE *data = vect->data;
	REBCNT bits  = VECT_TYPE(vect);
//	REBCNT dims  = vect->size >> 8;
	REBCNT len;
	REBCNT n;
	REBCNT c;
	REBVAL v;
	REBYTE buf[32];
	REBYTE l;
	REBOOL indented = !GET_MOPT(mold, MOPT_INDENT);

	if (GET_MOPT(mold, MOPT_MOLD_ALL)) {
		len = VAL_TAIL(value);
		n = 0;
	} else {
		len = VAL_LEN(value);
		n = VAL_INDEX(value);
	}

	if (molded) {
		REBCNT type = (bits >= VTSF32) ? REB_DECIMAL : REB_INTEGER;
		if (GET_MOPT(mold, MOPT_MOLD_ALL)) {
			Emit(mold, "#(T ", value);
			if (bits >= VTUI08 && bits <= VTUI64) Append_Bytes(mold->series, "unsigned ");
			Emit(mold, "N I I [", type + 1, VECT_BIT_SIZE(bits), len);
		}
		else {
			Emit(mold, "#(S [", Get_Sym_Name(SYM_INT8X + bits));
		}
		if (indented && len > 10) {
			mold->indent++;
			New_Indented_Line(mold);
		}
		CHECK_MOLD_LIMIT(mold, len);
	}

	c = 0;
	for (; n < vect->tail; n++) {
		if (MOLD_HAS_LIMIT(mold) && MOLD_OVER_LIMIT(mold)) return;
		get_vect(bits, data, n, &v);
		if (bits < VTSF08) {
			l = Emit_Integer(buf, VAL_INT64(&v));
		} else {
			l = Emit_Decimal(buf, VAL_DECIMAL(&v), 0, '.', mold->digits);
		}
		Append_Bytes_Len(mold->series, buf, l);
		if (indented && (++c > 9) && (n+1 < vect->tail)) {
			New_Indented_Line(mold);
			c = 0;
		}
		else
			Append_Byte(mold->series, ' '); 
	}

	if (len) mold->series->tail--; // remove final space

	if (molded) {
		if (indented && len > 10) {
			mold->indent--;
			New_Indented_Line(mold);
		}
		Append_Byte(mold->series, ']');
		if (GET_MOPT(mold, MOPT_MOLD_ALL) && VAL_INDEX(value)) {
			Append_Byte(mold->series, ' ');
			Append_Int(mold->series, VAL_INDEX(value) + 1);
		}
		Append_Byte(mold->series, ')');
	}
}
