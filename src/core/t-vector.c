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
**  Module:  t-vector.c
**  Summary: vector datatype
**  Section: datatypes
**  Author:  Carl Sassenrath, Oldes
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

static REBU64 f_to_u64(float n) {
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
static void get_vect(REBCNT type, REBYTE *data, REBCNT n, REBVAL *val) {
	ASSERT1(type <= VTSF64, RP_BAD_SIZE);
	getters[type](data, n, val);
}

FORCE_INLINE
static REBDEC get_vect_decimal(REBCNT type, REBYTE *data, REBCNT n) {
	ASSERT1(type <= VTSF64, RP_BAD_SIZE);
	REBVAL val;
	getters[type](data, n, &val);
	if (type >= VTSF08) return VAL_DECIMAL(&val);
	if (type <= VTUI64) return (REBDEC)VAL_INT64(&val);
	return (REBDEC)VAL_UNT64(&val);
}

FORCE_INLINE
static void set_vect(REBCNT type, REBYTE *data, REBCNT n, REBVAL *val) {
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

static void Query_Vector_Statictics(REBVAL *vect, REBVQV *out) {
	REBLEN len = VAL_LEN(vect);
	REBCNT type = VAL_VEC_INFO(vect);
	REBCNT n = 0;
	REBYTE *data = VAL_VEC_DATA(vect);
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
static REBDEC Query_Vector_Median(REBVAL *vec) {
	REBLEN len = VAL_TAIL(vec);
	REBCNT type = VAL_VEC_INFO(vec);
	REBSER *sorted;
	REBDEC median;

	if (len == 0) return 0;
	// Make a vector copy, because sorting modifies
	sorted = Copy_Binary(VAL_SERIES(vec));
	ASSERT1(type < VT_MAX, RP_ASSERTS);
	unstable_sort(SERIES_DATA(sorted), len, VAL_VEC_WIDE(vec), compares[type]);

	median = get_vect_decimal(type, SERIES_DATA(sorted), len/2);
	if (len%2 == 0) {
		// Even number of elements
		median = (get_vect_decimal(type, SERIES_DATA(sorted), len/2-1) + median) / 2.0;
	}
	Free_Series(sorted);
	return median;
}


FORCE_INLINE
static void Set_Vector_Value(REBCNT type, REBYTE *data, REBCNT n, REBVAL *val) {
	REBVAL num = *val; // because may be modified!
	if (IS_DECIMAL(val)) {
		// value is decimal
		if (type <= VTUI64) {
			// but target is integer 
			VAL_INT64(&num) = (REBI64)VAL_DECIMAL(val);
		}
	}
	else if (IS_INTEGER(val) || IS_CHAR(val)) {
		if (type > VTUI64) {
			VAL_DECIMAL(&num) = (REBDEC)VAL_INT64(val);
		}
	}
	else Trap_Arg(val);
	setters[type](data, n, &num);
}


void Set_Vector_Row(REBSER *ser, REBVAL *blk, REBCNT type)
{
	REBVAL *val;
	REBLEN n = 0;
	REBCNT len = VAL_LEN(blk);

	if (IS_BLOCK(blk)) {
		val = VAL_BLK_DATA(blk);
		for (; NOT_END(val); val++) {
			Set_Vector_Value(type, ser->data, n++, val);
		}
	}
	else {
#ifdef old_code
		REBYTE *data = VAL_BIN_DATA(blk);
		for (; len > 0; len--, idx++) {
			set_vect(type, ser->data, n++, (REBI64)(data[idx]), f);
		}
#else
		REBCNT bytes = ser->tail * SERIES_WIDE(ser); //TODO: review! Wide is max 256 bytes!!!
		if (len > bytes) len = bytes;
		COPY_MEM(ser->data, VAL_BIN_DATA(blk), len);
#endif
	}
}

void Find_Minimum_Of_Vector(REBVAL *vect, REBVAL *ret) {
	REBLEN len;
	REBYTE *data;
	
	len = VAL_LEN(vect);

	SET_NONE(ret);
	if (len == 0) return;

#define FIND_MIN(type, set) {             \
        type *typed_data = (type *)data;     \
        type min_value = typed_data[0];      \
        for (REBLEN i = 1; i < len; i++) {   \
            min_value = (typed_data[i] < min_value) \
			          ?  typed_data[i] : min_value; \
        }                                    \
        set(ret, min_value);         \
        return;                              \
    }

	data = VAL_VEC_DATA(vect);

	switch (VAL_VEC_INFO(vect)) {
	case VTSI08: FIND_MIN(i8, SET_INTEGER); break;
	case VTSI16: FIND_MIN(i16, SET_INTEGER); break;
	case VTSI32: FIND_MIN(i32, SET_INTEGER); break;
	case VTSI64: FIND_MIN(i64, SET_INTEGER); break;
	case VTUI08: FIND_MIN(u8, SET_INTEGER); break;
	case VTUI16: FIND_MIN(u16, SET_INTEGER); break;
	case VTUI32: FIND_MIN(u32, SET_INTEGER); break;
	case VTUI64: FIND_MIN(u64, SET_INTEGER); break;
	case VTSF32: FIND_MIN(float, SET_DECIMAL); break;
	case VTSF64: FIND_MIN(double, SET_DECIMAL); break;
	}

#undef FIND_MIN
}

void Find_Maximum_Of_Vector(REBVAL *vect, REBVAL *ret) {
	REBLEN len;
	REBYTE *data;

	len = VAL_LEN(vect);

	SET_NONE(ret);
	if (len == 0) return;

#define FIND_MAX(type, set) {             \
        type *typed_data = (type *)data;     \
        type max_value = typed_data[0];      \
        for (REBLEN i = 1; i < len; i++) {   \
            max_value = (typed_data[i] > max_value) \
                      ?  typed_data[i] : max_value; \
        }                                    \
        set(ret, max_value);         \
        return;                              \
    }

	data = VAL_VEC_DATA(vect);

	switch (VAL_VEC_INFO(vect)) {
	case VTSI08: FIND_MAX(i8, SET_INTEGER); break;
	case VTSI16: FIND_MAX(i16, SET_INTEGER); break;
	case VTSI32: FIND_MAX(i32, SET_INTEGER); break;
	case VTSI64: FIND_MAX(i64, SET_INTEGER); break;
	case VTUI08: FIND_MAX(u8, SET_INTEGER); break;
	case VTUI16: FIND_MAX(u16, SET_INTEGER); break;
	case VTUI32: FIND_MAX(u32, SET_INTEGER); break;
	case VTUI64: FIND_MAX(u64, SET_INTEGER); break;
	case VTSF32: FIND_MAX(float, SET_DECIMAL); break;
	case VTSF64: FIND_MAX(double, SET_DECIMAL); break;
	}

#undef FIND_MAX
}


/***********************************************************************
**
*/	static REBOOL Query_Vector_Field(REBVAL *vec, REBCNT field, REBVAL *ret, REBVQV *vqv)
/*
**		Set a value with requested vector field result 
**
***********************************************************************/
{
#define RETURN_DECIMAL(v) {SET_DECIMAL(ret, v); return TRUE;}
#define RETURN_NUMBER(v)  {SET_DECIMAL(ret, v); goto return_number;}
	
	REBCNT type = VAL_VEC_INFO(vec);

	switch (field) {
	case SYM_TYPE:
		Init_Word(ret, (type >= VTSF08) ? SYM_DECIMAL_TYPE : SYM_INTEGER_TYPE);
		break;
	case SYM_SIZE:
		SET_INTEGER(ret, VAL_VEC_BITS(vec));
		break;
	case SYM_LENGTH:
		SET_INTEGER(ret, VAL_TAIL(vec));
		break;
	case SYM_SIGNED:
		SET_LOGIC(ret, VAL_VEC_SIGN(vec));
		break;
	case SYM_MIN:
	case SYM_MINIMUM:
		if (vqv) RETURN_NUMBER(vqv->minimum);
		Find_Minimum_Of_Vector(vec, ret);
		break;
	case SYM_MAX:
	case SYM_MAXIMUM:
		if (vqv) RETURN_NUMBER(vqv->maximum);
		Find_Maximum_Of_Vector(vec, ret);
		break;
	default:
		if (!vqv) {
			REBVQV out;
			Query_Vector_Statictics(vec, &out);
			vqv = &out;
		}
		if (field == SYM_RANGE) RETURN_NUMBER((vqv->maximum - vqv->minimum));
		if (field == SYM_SUM) RETURN_NUMBER(vqv->sum);
		if (field == SYM_MEAN || field == SYM_AVERAGE) RETURN_DECIMAL(vqv->mean);
		if (field == SYM_MEDIAN) RETURN_DECIMAL(Query_Vector_Median(vec));
		if (field == SYM_VARIANCE) RETURN_DECIMAL(vqv->variance);
		if (field == SYM_POPULATION_DEVIATION) RETURN_DECIMAL(sqrt(vqv->variance / VAL_TAIL(vec)));
		if (field == SYM_SAMPLE_DEVIATION) RETURN_DECIMAL(sqrt(vqv->variance / (VAL_TAIL(vec) - 1)));
		return FALSE;
	}
	return TRUE;
return_number:
	// Return integer if vector type is integer, else keep decimal
	if (type < VTSF08) SET_INTEGER(ret, (REBI64)VAL_DECIMAL(ret));
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
	REBYTE *data = VAL_VEC_HEAD(vect);
	REBCNT type = VAL_VEC_INFO(vect);
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
// Helper macro to generate per-type math code
#define VEC_OP_LOOP(type, op, val) \
    do { \
        type *p = (type*)data; \
        for (REBCNT j = n; j < len; ++j) p[j] op (type)(val); \
    } while (0)

/***********************************************************************
**
*/	void Math_Op_Vector(REBVAL *out, REBVAL *v1, REBVAL *v2, REBCNT action)
/*
**		Do basic math operation on a vector
**
***********************************************************************/
{
	REBYTE *data;
	REBCNT vtype;
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
		return;
	}

	vtype = VAL_VEC_INFO(left);
	len = VAL_LEN(left);


	if (IS_INTEGER(right)) {
		i = VAL_INT64(right);
		f = (REBDEC)i;
	} else {
		f = VAL_DECIMAL(right);
		i = (REBI64)f;
	}

	SET_VECTOR(out, Copy_Binary_Part(VAL_SERIES(left), VAL_INDEX(left), len), vtype);
	data = VAL_VEC_HEAD(out);

	switch (action) {
	case A_ADD:
		switch (vtype) {
		case VTSI08: VEC_OP_LOOP(i8, +=, i); break;
		case VTSI16: VEC_OP_LOOP(i16, +=, i); break;
		case VTSI32: VEC_OP_LOOP(i32, +=, i); break;
		case VTSI64: VEC_OP_LOOP(i64, +=, i); break;
		case VTUI08: VEC_OP_LOOP(u8, +=, i); break;
		case VTUI16: VEC_OP_LOOP(u16, +=, i); break;
		case VTUI32: VEC_OP_LOOP(u32, +=, i); break;
		case VTUI64: VEC_OP_LOOP(u64, +=, i); break;
		case VTSF32: VEC_OP_LOOP(float, +=, f); break;
		case VTSF64: VEC_OP_LOOP(double, +=, f); break;
		}
		break;
	case A_SUBTRACT:
		switch (vtype) {
		case VTSI08: VEC_OP_LOOP(i8, -=, i); break;
		case VTSI16: VEC_OP_LOOP(i16, -=, i); break;
		case VTSI32: VEC_OP_LOOP(i32, -=, i); break;
		case VTSI64: VEC_OP_LOOP(i64, -=, i); break;
		case VTUI08: VEC_OP_LOOP(u8, -=, i); break;
		case VTUI16: VEC_OP_LOOP(u16, -=, i); break;
		case VTUI32: VEC_OP_LOOP(u32, -=, i); break;
		case VTUI64: VEC_OP_LOOP(u64, -=, i); break;
		case VTSF32: VEC_OP_LOOP(float, -=, f); break;
		case VTSF64: VEC_OP_LOOP(double, -=, f); break;
		}
		break;
	case A_MULTIPLY:
		switch (vtype) {
		case VTSI08: VEC_OP_LOOP(i8, *=, i); break;
		case VTSI16: VEC_OP_LOOP(i16, *=, i); break;
		case VTSI32: VEC_OP_LOOP(i32, *=, i); break;
		case VTSI64: VEC_OP_LOOP(i64, *=, i); break;
		case VTUI08: VEC_OP_LOOP(u8, *=, i); break;
		case VTUI16: VEC_OP_LOOP(u16, *=, i); break;
		case VTUI32: VEC_OP_LOOP(u32, *=, i); break;
		case VTUI64: VEC_OP_LOOP(u64, *=, i); break;
		case VTSF32: VEC_OP_LOOP(float, *=, f); break;
		case VTSF64: VEC_OP_LOOP(double, *=, f); break;
		}
		break;
	case A_DIVIDE:
		if (i == 0 && vtype <= VTUI64) Trap0(RE_ZERO_DIVIDE);
		switch (vtype) {
		case VTSI08: VEC_OP_LOOP(i8, /=, i); break;
		case VTSI16: VEC_OP_LOOP(i16, /=, i); break;
		case VTSI32: VEC_OP_LOOP(i32, /=, i); break;
		case VTSI64: VEC_OP_LOOP(i64, /=, i); break;
		case VTUI08: VEC_OP_LOOP(u8, /=, i); break;
		case VTUI16: VEC_OP_LOOP(u16, /=, i); break;
		case VTUI32: VEC_OP_LOOP(u32, /=, i); break;
		case VTUI64: VEC_OP_LOOP(u64, /=, i); break;
		case VTSF32: VEC_OP_LOOP(float, /=, f); break;
		case VTSF64: VEC_OP_LOOP(double, /=, f); break;
		}
		break;
	case A_AND:
		switch (vtype) {
		case VTSI08: VEC_OP_LOOP(i8, &=, i); break;
		case VTSI16: VEC_OP_LOOP(i16, &=, i); break;
		case VTSI32: VEC_OP_LOOP(i32, &=, i); break;
		case VTSI64: VEC_OP_LOOP(i64, &=, i); break;
		case VTUI08: VEC_OP_LOOP(u8, &=, i); break;
		case VTUI16: VEC_OP_LOOP(u16, &=, i); break;
		case VTUI32: VEC_OP_LOOP(u32, &=, i); break;
		case VTUI64: VEC_OP_LOOP(u64, &=, i); break;
		default: Trap_Math_Args(REB_DECIMAL, action);
		}
		break;
	case A_OR:
		switch (vtype) {
		case VTSI08: VEC_OP_LOOP(i8, |=, i); break;
		case VTSI16: VEC_OP_LOOP(i16, |=, i); break;
		case VTSI32: VEC_OP_LOOP(i32, |=, i); break;
		case VTSI64: VEC_OP_LOOP(i64, |=, i); break;
		case VTUI08: VEC_OP_LOOP(u8, |=, i); break;
		case VTUI16: VEC_OP_LOOP(u16, |=, i); break;
		case VTUI32: VEC_OP_LOOP(u32, |=, i); break;
		case VTUI64: VEC_OP_LOOP(u64, |=, i); break;
		default: Trap_Math_Args(REB_DECIMAL, action);
		}
		break;
	case A_XOR:
		switch (vtype) {
		case VTSI08: VEC_OP_LOOP(i8, ^=, i); break;
		case VTSI16: VEC_OP_LOOP(i16, ^=, i); break;
		case VTSI32: VEC_OP_LOOP(i32, ^=, i); break;
		case VTSI64: VEC_OP_LOOP(i64, ^=, i); break;
		case VTUI08: VEC_OP_LOOP(u8, ^=, i); break;
		case VTUI16: VEC_OP_LOOP(u16, ^=, i); break;
		case VTUI32: VEC_OP_LOOP(u32, ^=, i); break;
		case VTUI64: VEC_OP_LOOP(u64, ^=, i); break;
		default: Trap_Math_Args(REB_DECIMAL, action);
		}
		break;
	case A_REMAINDER:
		if (i == 0) Trap0(RE_ZERO_DIVIDE);
		switch (vtype) {
		case VTSI08: VEC_OP_LOOP(i8, %=, i); break;
		case VTSI16: VEC_OP_LOOP(i16, %=, i); break;
		case VTSI32: VEC_OP_LOOP(i32, %=, i); break;
		case VTSI64: VEC_OP_LOOP(i64, %=, i); break;
		case VTUI08: VEC_OP_LOOP(u8, %=, i); break;
		case VTUI16: VEC_OP_LOOP(u16, %=, i); break;
		case VTUI32: VEC_OP_LOOP(u32, %=, i); break;
		case VTUI64: VEC_OP_LOOP(u64, %=, i); break;
		case VTSF32: for (REBCNT j = n; j < len; ++j) ((float *)data)[j] = fmodf(((float *)data)[j], (float)f); break;
		case VTSF64: for (REBCNT j = n; j < len; ++j) ((double *)data)[j] = fmod(((double *)data)[j], f); break;
		}
		break;
	}
	return;
}
#undef VEC_OP_LOOP

// Helper macro for elementwise vector ops
#define VEC_OP_LOOP(type, op) \
    do { \
		type *o = (type*)data; \
        type *p = (type*)data1; \
        type *q = (type*)data2; \
        for (REBCNT j = n; j < len; ++j) o[j] = p[idx1 + j] op q[idx2 + j]; \
    } while (0)
#define VEC_OP_LOOP_NO_ZERO(type, op) \
    do { \
		type *o = (type*)data; \
        type *p = (type*)data1; \
        type *q = (type*)data2; \
        for (REBCNT j = n; j < len; ++j) {\
			if (q[idx2 + j] == 0) Trap0(RE_ZERO_DIVIDE);\
			o[j] = p[idx1 + j] op q[idx2 + j];} \
    } while (0)

/***********************************************************************
**
*/	void Math_Op_Vector_Vector(REBVAL *out, REBVAL *v1, REBVAL *v2, REBCNT action)
/*
**		Do basic math operation on a vector
**
***********************************************************************/
{
	REBLEN len, n = 0;
	REBLEN idx1 = VAL_INDEX(v1);
	REBLEN idx2 = VAL_INDEX(v2);
	REBINT type = VAL_VEC_INFO(v1);
	REBYTE *data;
	REBYTE *data1 = VAL_VEC_HEAD(v1);
	REBYTE *data2 = VAL_VEC_HEAD(v2);

	len = MIN(VAL_LEN(v1), VAL_LEN(v2));

	if (type != VAL_VEC_INFO(v2)) Trap0(RE_VECTOR_NOT_COMPATIBLE);
	SET_VECTOR(out, Make_Series(MAX(len, 1), VAL_VEC_WIDE(v1), FALSE), type);
	VAL_TAIL(out) = len;
	data = VAL_VEC_HEAD(out);

	switch (action) {
	case A_ADD:
		switch (type) {
		case VTSI08: VEC_OP_LOOP(i8, +); break;
		case VTSI16: VEC_OP_LOOP(i16, +); break;
		case VTSI32: VEC_OP_LOOP(i32, +); break;
		case VTSI64: VEC_OP_LOOP(i64, +); break;
		case VTUI08: VEC_OP_LOOP(u8, +); break;
		case VTUI16: VEC_OP_LOOP(u16, +); break;
		case VTUI32: VEC_OP_LOOP(u32, +); break;
		case VTUI64: VEC_OP_LOOP(u64, +); break;
		case VTSF32: VEC_OP_LOOP(float, +); break;
		case VTSF64: VEC_OP_LOOP(double, +); break;
		}
		break;
	case A_SUBTRACT:
		switch (type) {
		case VTSI08: VEC_OP_LOOP(i8, -); break;
		case VTSI16: VEC_OP_LOOP(i16, -); break;
		case VTSI32: VEC_OP_LOOP(i32, -); break;
		case VTSI64: VEC_OP_LOOP(i64, -); break;
		case VTUI08: VEC_OP_LOOP(u8, -); break;
		case VTUI16: VEC_OP_LOOP(u16, -); break;
		case VTUI32: VEC_OP_LOOP(u32, -); break;
		case VTUI64: VEC_OP_LOOP(u64, -); break;
		case VTSF32: VEC_OP_LOOP(float, -); break;
		case VTSF64: VEC_OP_LOOP(double, -); break;
		}
		break;
	case A_MULTIPLY:
		switch (type) {
		case VTSI08: VEC_OP_LOOP(i8, *); break;
		case VTSI16: VEC_OP_LOOP(i16, *); break;
		case VTSI32: VEC_OP_LOOP(i32, *); break;
		case VTSI64: VEC_OP_LOOP(i64, *); break;
		case VTUI08: VEC_OP_LOOP(u8, *); break;
		case VTUI16: VEC_OP_LOOP(u16, *); break;
		case VTUI32: VEC_OP_LOOP(u32, *); break;
		case VTUI64: VEC_OP_LOOP(u64, *); break;
		case VTSF32: VEC_OP_LOOP(float, *); break;
		case VTSF64: VEC_OP_LOOP(double, *); break;
		}
		break;
	case A_DIVIDE:
		switch (type) {
		case VTSI08: VEC_OP_LOOP_NO_ZERO(i8, /); break;
		case VTSI16: VEC_OP_LOOP_NO_ZERO(i16, /); break;
		case VTSI32: VEC_OP_LOOP_NO_ZERO(i32, /); break;
		case VTSI64: VEC_OP_LOOP_NO_ZERO(i64, /); break;
		case VTUI08: VEC_OP_LOOP_NO_ZERO(u8, /); break;
		case VTUI16: VEC_OP_LOOP_NO_ZERO(u16, /); break;
		case VTUI32: VEC_OP_LOOP_NO_ZERO(u32, /); break;
		case VTUI64: VEC_OP_LOOP_NO_ZERO(u64, /); break;
		case VTSF32: VEC_OP_LOOP_NO_ZERO(float, /); break;
		case VTSF64: VEC_OP_LOOP_NO_ZERO(double, /); break;
		}
		break;
	case A_AND:
		switch (type) {
		case VTSI08: VEC_OP_LOOP(i8, &); break;
		case VTSI16: VEC_OP_LOOP(i16, &); break;
		case VTSI32: VEC_OP_LOOP(i32, &); break;
		case VTSI64: VEC_OP_LOOP(i64, &); break;
		case VTUI08: VEC_OP_LOOP(u8, &); break;
		case VTUI16: VEC_OP_LOOP(u16, &); break;
		case VTUI32: VEC_OP_LOOP(u32, &); break;
		case VTUI64: VEC_OP_LOOP(u64, &); break;
		default: Trap_Math_Args(REB_DECIMAL, action);
		}
		break;
	case A_OR:
		switch (type) {
		case VTSI08: VEC_OP_LOOP(i8, |); break;
		case VTSI16: VEC_OP_LOOP(i16, |); break;
		case VTSI32: VEC_OP_LOOP(i32, |); break;
		case VTSI64: VEC_OP_LOOP(i64, |); break;
		case VTUI08: VEC_OP_LOOP(u8, |); break;
		case VTUI16: VEC_OP_LOOP(u16, |); break;
		case VTUI32: VEC_OP_LOOP(u32, |); break;
		case VTUI64: VEC_OP_LOOP(u64, |); break;
		default: Trap_Math_Args(REB_DECIMAL, action);
		}
		break;
	case A_XOR:
		switch (type) {
		case VTSI08: VEC_OP_LOOP(i8, ^); break;
		case VTSI16: VEC_OP_LOOP(i16, ^); break;
		case VTSI32: VEC_OP_LOOP(i32, ^); break;
		case VTSI64: VEC_OP_LOOP(i64, ^); break;
		case VTUI08: VEC_OP_LOOP(u8, ^); break;
		case VTUI16: VEC_OP_LOOP(u16, ^); break;
		case VTUI32: VEC_OP_LOOP(u32, ^); break;
		case VTUI64: VEC_OP_LOOP(u64, ^); break;
		default: Trap_Math_Args(REB_DECIMAL, action);
		}
		break;
	case A_REMAINDER:
		switch (type) {
		case VTSI08: VEC_OP_LOOP_NO_ZERO(i8, %); break;
		case VTSI16: VEC_OP_LOOP_NO_ZERO(i16, %); break;
		case VTSI32: VEC_OP_LOOP_NO_ZERO(i32, %); break;
		case VTSI64: VEC_OP_LOOP_NO_ZERO(i64, %); break;
		case VTUI08: VEC_OP_LOOP_NO_ZERO(u8, %); break;
		case VTUI16: VEC_OP_LOOP_NO_ZERO(u16, %); break;
		case VTUI32: VEC_OP_LOOP_NO_ZERO(u32, %); break;
		case VTUI64: VEC_OP_LOOP_NO_ZERO(u64, %); break;
		case VTSF32: for (REBCNT j = n; j < len; ++j) ((float *)data)[j] = fmodf(((float *)data1)[idx1 + j], ((float *)data2)[idx2 + j]); break;
		case VTSF64: for (REBCNT j = n; j < len; ++j) ((double *)data)[j] = fmod(((double *)data1)[idx1 + j], ((double *)data2)[idx2+j]); break;
		}
		break;
	}
	return;
}
#undef VEC_OP_LOOP
#undef VEC_OP_LOOP_NO_ZERO
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
	REBVAL v1, v2;
	REBYTE *d1 = VAL_SERIES(a)->data;
	REBYTE *d2 = VAL_SERIES(b)->data;
	REBCNT  b1 = VAL_VEC_INFO(a);
	REBCNT  b2 = VAL_VEC_INFO(b);

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
	REBYTE *data = VAL_VEC_HEAD(vect);
	REBCNT type = VAL_VEC_INFO(vect);
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
*/	void Sort_Vector(REBVAL *vec, REBLEN len, REBFLG reversed)
/*
***********************************************************************/
{
	REBCNT  type = VAL_VEC_INFO(vec);
	REBCNT  skp  = VAL_VEC_WIDE(vec);
	REBYTE* data = VAL_VEC_DATA(vec);
	ASSERT1(type < VT_MAX, RP_ASSERTS);
	unstable_sort(data, len, skp, reversed ? compares_rev[type] : compares[type]);
}

/***********************************************************************
**
*/	void Get_Vector_Value(REBVAL *var, REBVAL *vec, REBCNT index)
/*
***********************************************************************/
{
	REBCNT type  = VAL_VEC_INFO(vec);
	get_vect(type, VAL_VEC_HEAD(vec), index, var);
	SET_TYPE(var, (type >= VTSF08) ? REB_DECIMAL : REB_INTEGER);
}

/***********************************************************************
**
*/	REBSER* Make_Vector_Series(REBINT size, REBCNT wide, REBINT dims)
/*
**		size: number of values
**		wide: number of bytes per value
**		dims: number of dimensions (currently always 1)
**
***********************************************************************/
{
	REBU64 len = (REBU64)size * dims;
	if (len > 0x7fffffff) return NULL;

	REBSER* ser = Make_Series(MAX(1,size), wide, TRUE); // !!! can width help extend the len?
	LABEL_SERIES(ser, "make vector");
	ser->tail = AS_REBLEN(len);
	return ser;
}

/***********************************************************************
**
*/	REBINT Make_Vector(REBVAL* val, REBCNT vtype, REBINT size)
/*
**		type: encoded vector type info (one of VTSI08..VTSF64)
**		size: number of values
**
***********************************************************************/
{
	REBSER* ser;
	if (!(ser = Make_Vector_Series(size, VECT_WIDE(vtype), VECT_DIMS(vtype))))
		return FALSE;
	
	SET_VECTOR(val, ser, vtype);
	//printf("Make_Vector: wide: %u bits: %u sign: %u\n", VAL_VEC_WIDE(val), VAL_VEC_BITS(val), VAL_VEC_SIGN(val));
	return TRUE;
}

static
REBCNT Get_Vector_Type_From_Symbol(REBCNT sym) {
	sym = Normalize_Vector_Type_Symbol(sym);
	return (sym < SYM_INT8X || sym > SYM_FLOAT64X)
		? UNKNOWN
		: sym - SYM_INT8X;
}

/***********************************************************************
**
*/	void Make_Vector_From_Word(REBVAL *val, REBCNT sym, REBINT size)
/*
**	Make a vector from a type name.
**
***********************************************************************/
{
	REBINT type = Get_Vector_Type_From_Symbol(sym);
	if (type==UNKNOWN || !Make_Vector(val, type, size)) {
		VAL_SERIES(val) = NULL;
	}
}

/***********************************************************************
**
*/	REBVAL *Construct_Vector(REBVAL *bp, REBVAL *value)
/*
**     Vector construction syntax. Supports only the new short variants.
**     #(type data index)
**
**     Fields:
**          type:  uint8!, uint16!, uint32!, uint64!,
**                 int8!, int16!, int32!, int64!,
*                  float32!, float64!
**    		data:  block of values or binary data
**          index: index in the created vector series
**
***********************************************************************/
{
	REBCNT size  = 0;
	REBLEN index = 0;
	REBVAL *iblk = 0;	

	// Vector type:
	if (!IS_WORD(bp)) return 0;
	if (VAL_WORD_CANON(bp) == SYM_VECTOR_TYPE) {
		// allow #(vector! uint8! [1 2 3])
		bp++;
		if (!IS_WORD(bp)) return 0;
	}
	REBCNT vtype = Get_Vector_Type_From_Symbol(VAL_WORD_CANON(bp));
	if (vtype == UNKNOWN) return 0;
	//printf("vtype: wide: %u bits: %u sign: %u\n", VECT_WIDE(vtype), VECT_BITS(vtype), VECT_SIGN(vtype));

	bp++;
	// Initial data:
	if (IS_BLOCK(bp) || IS_BINARY(bp)) {
		REBCNT len = VAL_LEN(bp);
		if (IS_BINARY(bp)) len /= VECT_WIDE(vtype);
		if (len > size && size == 0) size = len;
		iblk = bp;
		bp++;
	}
	else if (IS_END(bp)) {
		size = 0;
	}
	else return 0;
	// Index offset:
	if (IS_INTEGER(bp)) {
		index = (Int32s(bp, 1) - 1);
	}
	
	Make_Vector(value, vtype, size);
	if (!VAL_SERIES(value)) return 0;
	if (iblk) Set_Vector_Row(VAL_SERIES(value), iblk, vtype);
	VAL_INDEX(value) = index;

	return value;
}

/***********************************************************************
**
*/	REBVAL *Make_Vector_Spec(REBVAL *spec, REBVAL *value)
/*
**	Make a vector from an extended block spec.
**
**     make vector! [uint8! [1 2 3]]
**     make vector! [uint8! :data]
**     make vector! [uint8! :size :data :index]
**
**     ; backwards compatibility versions:
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
**     ; it is possible to use also data directly like:
**     make vector! [1 2 3] ; 64bit signed integers
**     make vector! [1.0 2] ; 64bit decimals
**
***********************************************************************/
{
	REBVAL *bp = VAL_BLK_DATA(spec);
	REBCNT isfloat = 0;  // 0 = int,    1 = float
	REBCNT sign = 1;     // 1 = signed, 0 = unsigned
	REBINT dims = 1;
	REBINT bits = 64;
	REBCNT size = 0;
	REBLEN index = 0;
	REBSER *vect;
	REBVAL *iblk = 0;
	REBVAL *val;
	REBCNT vtype = UNKNOWN;

	if (IS_WORD(bp)) {
		// Using the prefered type like: make vector! [uint8! ...]
		vtype = Get_Vector_Type_From_Symbol(VAL_WORD_CANON(bp));
		if (vtype != UNKNOWN) {
			bp++;
			bits = VECT_BITS(vtype);
			goto size_spec;
		}
		// Old specification like: make vector! [unsigned integer! 8 ...]
		switch (VAL_WORD_CANON(bp)) {
		case SYM_UNSIGNED: sign = 0; bp++; break;
		case SYM_SIGNED:   sign = 1; bp++; break;
		}
	}
	else if (IS_INTEGER(bp) || IS_DECIMAL(bp)) {
		// make vector! [1 2 3]
		// make vector! [1.0 2.0 3.0]
		// using signed and 64 bits as a default
		isfloat = IS_INTEGER(bp) ? 0 : 1;
		sign = 1;
		size = VAL_LEN(spec);
		iblk = spec;
		goto data_spec;
	}

	// INTEGER! or DECIMAL!
	if (IS_WORD(bp)) {
		if (VAL_WORD_CANON(bp) == (REB_INTEGER+1)) // integer! symbol
			isfloat = 0;
		else if (VAL_WORD_CANON(bp) == (REB_DECIMAL+1)) { // decimal! symbol
			isfloat = 1;
			if (!sign) return 0;
		}
		else return 0;
		bp++;
	}

	// BITS
	if (IS_INTEGER(bp)) {
		bits = Int32(bp);
		if (
			(bits == 32 || bits == 64)
			||
			(isfloat == 0 && (bits == 8 || bits == 16))
		) bp++;
		else return 0;
	} else return 0;
	vtype = VECT_MAKE_TYPE(bits==64?3:bits>>4, sign, isfloat);

size_spec:
	// For size, data and index one can use get-words
	// eg: make vector! [uint8! :size :data :index]
	// All these values are optional!
	val = bp;
	if (IS_GET_WORD(val))
		val = Get_Var(val);
	// SIZE
	if (IS_INTEGER(val)) {
		size = Int32(val);
		if (size < 0) return 0;
		val = ++bp;
		if (IS_GET_WORD(val))
			val = Get_Var(val);
	}
	// Initial data:
	if (IS_BLOCK(val) || IS_BINARY(val)) {
		REBCNT len = VAL_LEN(val);
		if (IS_BINARY(val)) len /= (bits >> 3);
		if (len > size && size == 0) size = len;
		iblk = val;
		val = ++bp;
		if (IS_GET_WORD(val))
			val = Get_Var(val);
	}

	// Index offset:
	if (IS_INTEGER(val) || IS_DECIMAL(val)) {
		index = Int32s(val, 1) - 1;
		val = ++bp;
		if (IS_GET_WORD(val)) val = Get_Var(val);
	}

	if (NOT_END(val)) return 0;
data_spec:
	if (vtype == UNKNOWN) vtype = VECT_MAKE_TYPE(bits == 64 ? 3 : bits >> 4, sign, isfloat);
	Make_Vector(value, vtype, size);
	if (!VAL_SERIES(value)) return 0;
	if (iblk) Set_Vector_Row(VAL_SERIES(value), iblk, VAL_VEC_INFO(value));
	VAL_INDEX(value) = index;

	return value;
}


/***********************************************************************
**
*/	REBFLG MT_Vector(REBVAL *out, REBVAL *data, REBCNT type)
/*
**	NOTE: data are on stack, it is not a BLOCK value,
**        so it is not possible to use macros like VAL_TAIL
**
***********************************************************************/
{
	if (Construct_Vector(data, out)) return TRUE;
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
	if (mode >=  0) return (num == 0);
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
	REBVAL *vec = val;
	REBSER *vect = VAL_SERIES(val);
	REBINT vtype = VAL_VEC_INFO(val);
	REBINT n;
	//REBINT dims;
	
	REBYTE *vp;

	if (IS_INTEGER(sel) || IS_DECIMAL(sel)) {
		n = Int32(sel);
		if (n == 0) return (pvs->setval) ? PE_BAD_RANGE : PE_NONE; // allow PICK with zero index but not for POKE
		if (n < 0) n++;
	} else if (IS_WORD(sel)) {
		if (set == 0) {
			val = pvs->value = pvs->store;
			if(!Query_Vector_Field(vec, VAL_WORD_CANON(sel), val, NULL)) return PE_BAD_SELECT;
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
		get_vect(vtype, vp, n - 1, pvs->store);
		SET_TYPE(pvs->store, (vtype >= VTSF08) ? REB_DECIMAL : REB_INTEGER);
		return PE_USE;
	}

	//--- Set Value...
	TRAP_PROTECT(vect);

	if (n <= 0 || (REBCNT)n > vect->tail) return PE_BAD_RANGE;
	Set_Vector_Value(vtype, vp, n-1, set);
	return PE_OK;
}


static void reverse_vector(REBVAL *value, REBCNT len)
{
	REBCNT n;
	REBCNT m;
	REBINT width = VAL_VEC_WIDE(value);

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
	REBLEN index;
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
	case A_OR:
	case A_AND:
	case A_XOR:
	case A_REMAINDER:
		if (IS_VECTOR(value) && IS_VECTOR(arg))
			Math_Op_Vector_Vector(D_RET, value, arg, action);
		else 
			Math_Op_Vector(D_RET, value, arg, action);
		return R_RET;
#endif

	case A_MAKE:
		// We only allow MAKE VECTOR! ...
		if (!IS_DATATYPE(value)) goto bad_make;

		// CASE: make vector! 100
		if (IS_INTEGER(arg) || IS_DECIMAL(arg)) {
			size = Int32s(arg, 0);
			if (size < 0) goto bad_make;
			Make_Vector(value, VTSI32, size);
			break;
		}
		// fall thru

	case A_TO:
		// CASE: make vector! #{01FF} ;== #(uint8! [1 255]) 
		if (IS_BINARY(arg)) {
			len = VAL_LEN(arg);
			Make_Vector(value, VTUI08, len);
			if (len > 0 && VAL_TAIL(value) == len) {
				COPY_MEM(VAL_VEC_HEAD(value), VAL_BIN_DATA(arg), len);
			}
			break;
		}
		// CASE: make vector! [...]
		if (IS_BLOCK(arg) && Make_Vector_Spec(arg, value)) break;
		goto bad_make;

	case A_LENGTHQ:
		//bits = 1 << (vect->size & 3);
		SET_INTEGER(D_RET, vect->tail);
		return R_RET;

	case A_COPY:
		len = Partial(value, 0, D_ARG(3), 0); // Can modify value index.
		ser = Copy_Binary_Part(vect, VAL_INDEX(value), len);
		ser->size = vect->size; // attributes
		SET_VECTOR(value, ser, VAL_VEC_INFO(value));
		break;

	case A_REVERSE:
		len = Partial(value, 0, D_ARG(3), 0);
		if (len > 0) reverse_vector(value, len);
		break;

	case A_SORT:
		len = Partial(value, 0, D_ARG(8), 0);
		if (
		//	D_REF(2) ||	// case sensitive
			D_REF(3) ||	// skip
			D_REF(5) 	// comparator
		//	D_REF(9) 	// all fields
			) Trap0(RE_FEATURE_NA);
		Sort_Vector(value, len, D_REF(10));
		break;
			
	case A_RANDOM:
		if (D_REF(2) || D_REF(4)) Trap0(RE_BAD_REFINES); // /seed /only
		Shuffle_Vector(value, D_REF(3));
		return R_ARG1;

	case A_REFLECT:
		bits = VAL_VEC_INFO(value);
		if (SYM_SPEC == VAL_WORD_SYM(D_ARG(2))) {
			blk = Make_Block(4);
			if (bits >= VTUI08 && bits <= VTUI64) Init_Word(Append_Value(blk), SYM_UNSIGNED);
			Query_Vector_Field(value, SYM_TYPE, Append_Value(blk), NULL);
			Query_Vector_Field(value, SYM_SIZE, Append_Value(blk), NULL);
			Query_Vector_Field(value, SYM_LENGTH, Append_Value(blk), NULL);
			Set_Series(REB_BLOCK, value, blk);
		} else {
			if(!Query_Vector_Field(value, VAL_WORD_SYM(D_ARG(2)), value, NULL))
				Trap_Reflect(VAL_TYPE(value), D_ARG(2));
		}
		break;

	case A_QUERY:
		bits = VAL_VEC_INFO(value);
		REBVAL *spec = Get_System(SYS_STANDARD, STD_VECTOR_INFO);
		if (!IS_OBJECT(spec)) Trap_Arg(spec);
		REBVAL *field = D_ARG(ARG_QUERY_FIELD);
		if(IS_WORD(field)) {
			if (!Query_Vector_Field(value, VAL_WORD_SYM(field), value, NULL))
				Trap_Reflect(VAL_TYPE(value), field); // better error?
			break;
		}
		REBVQV results = { 0 };
		Query_Vector_Statictics(value, &results);

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
					if (!Query_Vector_Field(value, VAL_WORD_SYM(word), val, &results))
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
			Query_Vector_Field(value, SYM_SIGNED, OFV(obj, STD_VECTOR_INFO_SIGNED), &results);
			Query_Vector_Field(value, SYM_TYPE,   OFV(obj, STD_VECTOR_INFO_TYPE), &results);
			Query_Vector_Field(value, SYM_SIZE,   OFV(obj, STD_VECTOR_INFO_SIZE), &results);
			Query_Vector_Field(value, SYM_LENGTH, OFV(obj, STD_VECTOR_INFO_LENGTH), &results);
			Query_Vector_Field(value, SYM_MINIMUM, OFV(obj, STD_VECTOR_INFO_MINIMUM), &results);
			Query_Vector_Field(value, SYM_MAXIMUM, OFV(obj, STD_VECTOR_INFO_MAXIMUM), &results);
			Query_Vector_Field(value, SYM_RANGE, OFV(obj, STD_VECTOR_INFO_RANGE), &results);
			Query_Vector_Field(value, SYM_SUM, OFV(obj, STD_VECTOR_INFO_SUM), &results);
			Query_Vector_Field(value, SYM_MEAN, OFV(obj, STD_VECTOR_INFO_MEAN), &results);
			Query_Vector_Field(value, SYM_MEDIAN, OFV(obj, STD_VECTOR_INFO_MEDIAN), &results);
			Query_Vector_Field(value, SYM_VARIANCE, OFV(obj, STD_VECTOR_INFO_VARIANCE), &results);
			Query_Vector_Field(value, SYM_POPULATION_DEVIATION, OFV(obj, STD_VECTOR_INFO_POPULATION_DEVIATION), &results);
			Query_Vector_Field(value, SYM_SAMPLE_DEVIATION, OFV(obj, STD_VECTOR_INFO_SAMPLE_DEVIATION), &results);
			SET_OBJECT(value, obj);
		}
		break;
	
	//-- Modification:
	case A_APPEND:
	case A_INSERT:
	case A_CHANGE:
		// Length of target (may modify index): (arg can be anything)
		len = Partial1((action == A_CHANGE) ? value : arg, DS_ARG(AN_LENGTH));
		index = VAL_INDEX(value);
		REBFLG args = 0;
		if (DS_REF(AN_PART)) SET_FLAG(args, AN_PART);
		index = Modify_Vector(action, value, index, arg, args, len, DS_REF(AN_DUP) ? Int32(DS_ARG(AN_COUNT)) : 1);
		VAL_INDEX(value) = index;
		break;

	case A_CLEAR:
		index = VAL_INDEX(value);
		if (index < VAL_TAIL(value)) {
			// Null all values.
			CLEAR(VAL_BIN_DATA(value), VAL_TAIL(value) - index);
			// Set new tail.
			VAL_TAIL(value) = index;
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
*/	REBCNT Modify_Vector(REBCNT action, REBVAL *vec, REBCNT index, REBVAL *src_val, REBCNT flags, REBINT dst_len, REBINT dups)
/*
**		action: INSERT, APPEND, CHANGE
**
**		vect:	    target
**		index:      position (in values)
**		src_val:	source
**		flags:		AN_PART
**		dst_len:	length to remove (in bytes)
**		dups:		dup count
**
**		return: new dst_idx
**
***********************************************************************/
{
	REBSER *src_ser = 0;
	REBCNT src_idx = 0;
	REBCNT src_len = 0;
	REBSER *vect = VAL_SERIES(vec);
	REBCNT type = VAL_VEC_INFO(vec);
	REBCNT tail = SERIES_TAIL(vect);
	REBCNT bpv  = VAL_VEC_WIDE(vec); // bytes per value
	REBINT size;  // total to insert/append/change (includes dups)
	REBVAL *val = NULL;

	if (dups < 0) return (action == A_APPEND) ? 0 : index;
	if (action == A_APPEND || index > tail) index = tail;

	// Use SCAN buffer as a temporary buffer.
	src_ser = BUF_SCAN;
	if (IS_VECTOR(src_val)) {
		REBLEN index = MIN(VAL_TAIL(src_val), VAL_INDEX(src_val));
		REBLEN part = VAL_TAIL(src_val) - index;
		if (action != A_CHANGE && GET_FLAG(flags, AN_PART) && dst_len < AS_INT(part))
			part = dst_len;
		if (type == VAL_VEC_INFO(src_val)) {
			// same vector types...
			src_ser = VAL_SERIES(src_val);
			src_idx = index;
			src_len = part;
		}
		else {
			// Make sure that the temp buffer is large enough.
			RESIZE_SERIES(src_ser, part * bpv);
			// Encode values from the source vector to the temp buffer.
			for (REBVAL tmp; src_len < part; index++) {
				Get_Vector_Value(&tmp, src_val, index);
				Set_Vector_Value(type, src_ser->data, src_len++, &tmp);
			}
		}
	}
	else if (IS_BINARY(src_val)) {
		src_ser = VAL_SERIES(src_val);
		src_idx = VAL_INDEX(src_val);
		src_len = (VAL_TAIL(src_val) - src_idx);
		if (action != A_CHANGE && GET_FLAG(flags, AN_PART) && dst_len < AS_INT(src_len))
			src_len = dst_len;
		src_len /= bpv;
		if (src_len == 0) Trap1(RE_INVALID_DATA, src_val);
	}
	else if (IS_BLOCK(src_val)) {
		REBLEN index = MIN(VAL_TAIL(src_val), VAL_INDEX(src_val));
		REBLEN part = VAL_TAIL(src_val) - index;
		// For INSERT or APPEND with /PART use the dst_len not src_len:
		if (action != A_CHANGE && GET_FLAG(flags, AN_PART))
			part = dst_len;
		// Make sure that the temp buffer is large enough.
		RESIZE_SERIES(src_ser, part * bpv);
		// Encode values from the block vector to the temp buffer.
		for (val = VAL_BLK_DATA(src_val); src_len < part; val++) {
			Set_Vector_Value(type, src_ser->data, src_len++, val);
		}
	}
	else {
		// Encode single value into the temp buffer.
		Set_Vector_Value(type, src_ser->data, src_len++, src_val);
	}

	// Total to insert:
	size = dups * src_len;

	if (action != A_CHANGE) {
		// Always expand vect for INSERT and APPEND actions:
		Expand_Series(vect, index, size);
	}
	else {
		// CHANGE action...
		if (size > dst_len)
			Expand_Series(vect, index, size - dst_len);
		else if (size < dst_len &&GET_FLAG(flags, AN_PART))
			Remove_Series(vect, index, dst_len - size);
	}

	// For dup count:
	for (; dups > 0; dups--) {
		// Don't use Insert_String as we may be inserting to a binary!
		// Destination is already expanded above.
		COPY_MEM(BIN_SKIP(vect, index * bpv), BIN_SKIP(src_ser, src_idx), src_len * bpv);
		index += src_len;
	}

	return (action == A_APPEND) ? 0 : index;
}

/***********************************************************************
**
*/	void Mold_Vector(REBVAL *value, REB_MOLD *mold, REBFLG molded)
/*
***********************************************************************/
{
	REBSER *vect = VAL_SERIES(value);
	REBYTE *data = vect->data;
	REBCNT type  = VAL_VEC_INFO(value);
//	REBCNT dims  = vect->size >> 8;
	REBCNT len, n, c;
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
		Emit(mold, "#(S [", Get_Sym_Name(SYM_INT8X + type));
		if (indented && len > 10) {
			mold->indent++;
			New_Indented_Line(mold);
		}
		CHECK_MOLD_LIMIT(mold, len);
	}

	c = 0;
	for (; n < vect->tail; n++) {
		if (MOLD_HAS_LIMIT(mold) && MOLD_OVER_LIMIT(mold)) return;
		get_vect(type, data, n, &v);
		if (type < VTSF08) {
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
