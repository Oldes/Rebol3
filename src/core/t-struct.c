/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2014 Atronix Engineering, Inc.
**  Copyright 2021-2025 Rebol Open Source Contributors
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
**  Module:  t-strut.c
**  Summary: C struct object datatype
**  Section: datatypes
**  Author:  Shixin Zeng, Oldes
**  Notes:
**
***********************************************************************/

#include "sys-core.h"

#define IS_INTEGER_TYPE(t) ((t) < STRUCT_TYPE_INTEGER)
#define IS_DECIMAL_TYPE(t) ((t) > STRUCT_TYPE_INTEGER && (t) < STRUCT_TYPE_DECIMAL)
#define IS_NUMERIC_TYPE(t) (IS_INTEGER_TYPE(t) || IS_DECIMAL_TYPE(t))

REBFLG MT_Struct(REBVAL *out, REBVAL *data, REBCNT type);
static void init_fields(REBVAL *ret, REBVAL *spec);

static const REBINT type_to_sym [STRUCT_TYPE_MAX] = {
	SYM_UINT8X,
	SYM_INT8X,
	SYM_UINT16X,
	SYM_INT16X,
	SYM_UINT32X,
	SYM_INT32X,
	SYM_UINT64X,
	SYM_INT64X,
	-1, //SYM_INTEGER,

	SYM_FLOATX,
	SYM_DOUBLEX,
	-1, //SYM_DECIMAL,

	SYM_POINTER,
	-1, //SYM_STRUCT
	SYM_WORD_TYPE,
	SYM_REBVALX
};

static REBFLG get_scalar(REBSTU *stu,
				  REBSTF *field,
				  REBCNT n, /* element index, starting from 0 */
				  REBVAL *val)
{
	REBYTE *data = STRUCT_DATA_BIN(stu) + field->offset + n * field->size;
	switch (field->type) {
		case STRUCT_TYPE_UINT8:
			SET_INTEGER(val, *(u8*)data);
			break;
		case STRUCT_TYPE_INT8:
			SET_INTEGER(val, *(i8*)data);
			break;
		case STRUCT_TYPE_UINT16:
			SET_INTEGER(val, *(u16*)data);
			break;
		case STRUCT_TYPE_INT16:
			SET_INTEGER(val, *(i16*)data);
			break;
		case STRUCT_TYPE_UINT32:
			SET_INTEGER(val, *(u32*)data);
			break;
		case STRUCT_TYPE_INT32:
			SET_INTEGER(val, *(i32*)data);
			break;
		case STRUCT_TYPE_UINT64:
			SET_INTEGER(val, *(u64*)data);
			break;
		case STRUCT_TYPE_INT64:
			SET_INTEGER(val, *(i64*)data);
			break;
		case STRUCT_TYPE_FLOAT:
			SET_DECIMAL(val, *(float*)data);
			break;
		case STRUCT_TYPE_DOUBLE:
			SET_DECIMAL(val, *(double*)data);
			break;
		case STRUCT_TYPE_POINTER:
			SET_INTEGER(val, (u64)*(void**)data);
			break;
		case STRUCT_TYPE_STRUCT:
			{
				SET_TYPE(val, REB_STRUCT);
				VAL_STRUCT_SPEC(val) = field->spec;
				VAL_STRUCT_DATA(val) = stu->data;
				VAL_STRUCT_OFFSET(val) = field->offset + n * field->size;
				VAL_STRUCT_SIZE(val) = field->size;
			}
			break;

		case STRUCT_TYPE_WORD:
			if (*(REBINT *)data == 0)
				SET_NONE(val);
			else 
				Set_Word(val, *(REBINT *)data, NULL, 0);
			break;
		case STRUCT_TYPE_REBVAL:
			if (*(REBINT *)data == 0)
				SET_NONE(val);
			else
				COPY_MEM(val, data, sizeof(REBVAL));
			break;
		default:
			/* should never be here */
			return FALSE;
	}
	return TRUE;
}

/***********************************************************************
**
*/	static REBFLG Get_Struct_Var(REBSTU *stu, REBVAL *word, REBVAL *val)
/*
***********************************************************************/
{
	REBSTF *field = NULL;
	REBCNT i = 0;
	field = STRUCT_FIELDS(stu);
	for (i = 0; i < STRUCT_FIELDS_NUM(stu); i ++, field ++) {
		if (VAL_WORD_CANON(word) == VAL_SYM_CANON(BLK_SKIP(PG_Word_Table.series, field->sym))) {
			if (field->array) {
				REBSER *ser;
				REBINT type = field->type;
				REBCNT sym = (REBCNT)type_to_sym[type];

				if (type > STRUCT_TYPE_DOUBLE || sym == NOT_FOUND) {
					// Using block as a result...
					ser = Make_Block(field->dimension);
					REBCNT n = 0;
					SET_TYPE(val, REB_BLOCK);
					for (n = 0; n < field->dimension; n++) {
						REBVAL elem;
						get_scalar(stu, field, n, &elem);
						Append_Val(ser, &elem);
					}
				}
				else {
					// Using vector as a result...
					ser = Make_Vector_From_Word(sym, field->dimension);
					COPY_MEM(SERIES_DATA(ser), STRUCT_DATA_BIN(stu) + field->offset, field->dimension * field->size);
					SET_TYPE(val, REB_VECTOR);
				}
				VAL_SERIES(val) = ser;
				VAL_INDEX(val) = 0;
			} else {
				get_scalar(stu, field, 0, val);
			}
			return TRUE;
		}
	}
	return FALSE;
}

/***********************************************************************
**
*/	void Get_Struct_Reflect(REBVAL *ret, REBSTU *stu, REBCNT type)
/*
***********************************************************************/
{
	REBVAL* val = NULL;
	REBVAL* type_blk = NULL;
	REBSER* out, * dim;
	REBSTF* field = STRUCT_FIELDS(stu);
	REBCNT i, n, cnt;

	cnt = STRUCT_FIELDS_NUM(stu);
	out = Make_Block(cnt * (type==SYM_BODY?2:1));
	Set_Block(ret, out);
	LABEL_SERIES(out, "struct_body");

	for (i = 0; i < cnt; i++, field++) {
		if (type != SYM_VALUES) {
			val = Append_Value(out);
			Init_Word(val, field->sym);
			if (type == SYM_WORDS) {
				SET_TYPE(val, REB_WORD);
			} else {
				SET_TYPE(val, REB_SET_WORD);
				VAL_SET_LINE(val);
			}
		}
		if (type != SYM_WORDS) {
			val = Append_Value(out);
			if (field->dimension > 1) {
				dim = Make_Block(field->dimension);
				SET_TYPE(val, REB_BLOCK);
				VAL_SERIES(val) = dim;
				for (n = 0; n < field->dimension; n++) {
					REBVAL* dv = Append_Value(dim);
					get_scalar(stu, field, n, dv);
				}
			}
			else {
				get_scalar(stu, field, 0, val);
			}
		}
	}
}

static REBOOL same_fields(REBSER *tgt, REBSER *src)
{
	if (SERIES_TAIL(tgt) != SERIES_TAIL(src)) {
		return FALSE;
	}
	REBSTF *tgt_fields = (REBSTF*) SERIES_DATA(tgt);
	REBSTF *src_fields = (REBSTF*) SERIES_DATA(src);
	REBSTI *tgt_info = (REBSTI*)tgt_fields;
	REBSTI *src_info = (REBSTI*)src_fields;
	REBCNT n;

	if (src_info->id && tgt_info->id == src_info->id && tgt_info->size == src_info->size)
		return TRUE;

	for(n = 1; n < SERIES_TAIL(src); n ++) {
		if (tgt_fields[n].type != src_fields[n].type) {
			return FALSE;
		}
		if (VAL_SYM_CANON(BLK_SKIP(PG_Word_Table.series, tgt_fields[n].sym))
			!= VAL_SYM_CANON(BLK_SKIP(PG_Word_Table.series, src_fields[n].sym))
			|| tgt_fields[n].offset != src_fields[n].offset
			|| tgt_fields[n].dimension != src_fields[n].dimension
			|| tgt_fields[n].size != src_fields[n].size) {
			return FALSE;
		}
		if (tgt_fields[n].type == STRUCT_TYPE_STRUCT
			&& ! same_fields(tgt_fields[n].spec->series, src_fields[n].spec->series)) {
			return FALSE;
		}
	}

	return TRUE;
}

static REBOOL assign_scalar(REBSTU *stu,
							REBSTF *field,
							REBCNT n, /* element index, starting from 0 */
							REBVAL *val)
{
	u64 i = 0;
	double d = 0;
	void *data = STRUCT_DATA_BIN(stu) + field->offset + n * field->size;

	if (field->type == STRUCT_TYPE_REBVAL) {
		COPY_MEM(data, val, sizeof(REBVAL));
		return TRUE;
	}

	switch (VAL_TYPE(val)) {
		case REB_DECIMAL:
			if (!IS_NUMERIC_TYPE(field->type)) {
				Trap_Type(val);
			}
			d = VAL_DECIMAL(val);
			i = (u64) d;
			break;
		case REB_INTEGER:
			if (!IS_NUMERIC_TYPE(field->type)
				&& field->type != STRUCT_TYPE_POINTER) {
				Trap_Type(val);
			}
			i = (u64) VAL_INT64(val);
			d = (double)i;
			break;
		case REB_STRUCT:
			if (STRUCT_TYPE_STRUCT != field->type) {
				Trap_Type(val);
			}
			break;
		case REB_WORD:
			if (STRUCT_TYPE_WORD != field->type) {
				Trap_Type(val);
			}
			i = (u64)VAL_WORD_SYM(val);
			break;
		case REB_BLOCK:
			if (STRUCT_TYPE_STRUCT == field->type) {
				DS_PUSH_NONE;
				SET_TYPE(DS_TOP, REB_STRUCT);
				VAL_STRUCT_SPEC(DS_TOP) = field->spec;
				VAL_STRUCT_DATA(DS_TOP) = stu->data;
				VAL_STRUCT_OFFSET(DS_TOP) = field->offset + n * field->size;
				VAL_STRUCT_SIZE(DS_TOP) = field->size;
				init_fields(DS_TOP, val);
				DS_POP;
			}
			else Trap_Type(val);
			return TRUE;
		default:
			Trap_Type(val);
	}

	switch (field->type) {
		case STRUCT_TYPE_INT8:
			*(i8*)data = (i8)i;
			break;
		case STRUCT_TYPE_UINT8:
			*(u8*)data = (u8)i;
			break;
		case STRUCT_TYPE_INT16:
			*(i16*)data = (i16)i;
			break;
		case STRUCT_TYPE_UINT16:
			*(u16*)data = (u16)i;
			break;
		case STRUCT_TYPE_INT32:
			*(i32*)data = (i32)i;
			break;
		case STRUCT_TYPE_UINT32:
		case STRUCT_TYPE_WORD:
			*(u32*)data = (u32)i;
			break;
		case STRUCT_TYPE_INT64:
			*(i64*)data = (i64)i;
			break;
		case STRUCT_TYPE_UINT64:
			*(u64*)data = (u64)i;
			break;
		case STRUCT_TYPE_POINTER:
			*(void**)data = (void*)i;
			break;
		case STRUCT_TYPE_FLOAT:
			*(float*)data = (float)d;
			break;
		case STRUCT_TYPE_DOUBLE:
			*(double*)data = (double)d;
			break;
		case STRUCT_TYPE_STRUCT:
			if (field->size != VAL_STRUCT_SIZE(val)) {
				Trap_Arg(val);
			}
			if (same_fields(field->spec->series, VAL_STRUCT_FIELDS(val))) {
				COPY_MEM(data, VAL_STRUCT_DATA_BIN(val), field->size);
			}
			else {
				Trap_Arg(val);
			}
			break;
		default:
			/* should never be here */
			return FALSE;
	}
	return TRUE;
}

/***********************************************************************
**
*/	static REBFLG Set_Struct_Var(REBSTU *stu, REBVAL *word, REBVAL *elem, REBVAL *val)
/*
***********************************************************************/
{
	REBSTF *field = STRUCT_FIELDS(stu);
	REBCNT i = 0;
	for (i = 0; i < STRUCT_FIELDS_NUM(stu); i ++, field ++) {
		if (VAL_WORD_CANON(word) == VAL_SYM_CANON(BLK_SKIP(PG_Word_Table.series, field->sym))) {
			if (field->array) {
				if (elem == NULL) { //set the whole array
					// Must be a block or vector with the same number of values as the array.
					if (!(IS_VECTOR(val) || IS_BLOCK(val)) || field->dimension != VAL_LEN(val)) {
						return FALSE;
					}
					if (IS_VECTOR(val)) {
						if (field->size != VAL_VEC_WIDTH(val)) {
							return FALSE;
						}
						COPY_MEM(STRUCT_DATA_BIN(stu) + field->offset, VAL_BIN_DATA(val), field->dimension * field->size);
					}
					else {
						// data in a block
						for (REBCNT n = 0; n < field->dimension; n++) {
							if (!assign_scalar(stu, field, n, VAL_BLK_SKIP(val, n))) {
								return FALSE;
							}
						}
					}

				} else {// set only one element
					if (!IS_INTEGER(elem)
						|| VAL_INT32(elem) <= 0
						|| VAL_UNT32(elem) > field->dimension) {
						return FALSE;
					}
					return assign_scalar(stu, field, VAL_INT32(elem) - 1, val);
				}
				return TRUE;
			} else {
				return assign_scalar(stu, field, 0, val);
			}
			return TRUE;
		}
	}
	return FALSE;
}

#ifdef unused_struct
/* parse struct attribute */
static void parse_attr (REBVAL *blk, REBINT *raw_size, REBUPT *raw_addr)
{
	*raw_size = -1;
	*raw_addr = 0;

	REBVAL *attr = VAL_BLK_DATA(blk);
	while (NOT_END(attr)) {
		if (IS_SET_WORD(attr)) {
			switch (VAL_WORD_CANON(attr)) {
				case SYM_RAW_SIZE:
					++ attr;
					if (IS_INTEGER(attr)) {
						if (*raw_size > 0) { /* duplicate raw-size */
							Trap_Arg(attr);
						}
						*raw_size = VAL_INT64(attr);
						if (*raw_size <= 0) {
							Trap_Arg(attr);
						}
					} else {
						Trap_Arg(attr);
					}
					break;
				case SYM_RAW_MEMORY:
					++ attr;
					if (IS_INTEGER(attr)) {
						if (*raw_addr != 0) { /* duplicate raw-memory */
							Trap_Arg(attr);
						}
						*raw_addr = VAL_UNT64(attr);
						if (*raw_addr == 0) {
							Trap_Arg(attr);
						}
					} else {
						Trap_Arg(attr);
					}
					break;
				case SYM_EXTERN:
#ifdef unused
					++ attr;

					if (*raw_addr != 0) /* raw-memory is exclusive with extern */
						Trap_Arg(attr);

					if (!IS_BLOCK(attr)
						|| VAL_LEN(attr) != 2) {
						Trap_Arg(attr);
					} else {
						REBVAL *lib;
						REBVAL *sym;
						void *addr;

						lib = VAL_BLK_SKIP(attr, 0);
						sym = VAL_BLK_SKIP(attr, 1);

						if (!IS_LIBRARY(lib))
							Trap_Arg(attr);
						if (IS_CLOSED_LIB(VAL_LIB_HANDLE(lib)))
							Trap0(RE_BAD_LIBRARY);
						if (!ANY_BINSTR(sym))
							Trap_Arg(sym);

						addr = OS_FIND_FUNCTION(LIB_FD(VAL_LIB_HANDLE(lib)), VAL_DATA(sym));
						if (!addr)
							Trap1(RE_SYMBOL_NOT_FOUND, sym);

						*raw_addr = (REBUPT)addr;
					}
#else
					Trap0(RE_FEATURE_NA);
#endif
					break;
					/*
					   case SYM_ALIGNMENT:
					   ++ attr;
					   if (IS_INTEGER(attr)) {
					   alignment = VAL_INT64(attr);
					   } else {
					   Trap_Arg(attr);
					   }
					   break;
					   */
				default:
					Trap_Arg(attr);
			}
		} else {
			Trap_Arg(attr);
		}
		++ attr;
	}
}

/* set storage memory to external addr: raw_addr */
static void set_ext_storage (REBVAL *out, REBINT raw_size, REBUPT raw_addr)
{
	REBSER *ser = NULL;

	if (raw_size >= 0 && raw_size != VAL_STRUCT_SIZE(out)) {
		Trap0(RE_INVALID_DATA);
	}

	ser = (REBSER *)Make_Node(SERIES_POOL);
	Prop_Series(ser, VAL_STRUCT_DATA_BIN(out));
	ser->data = (REBYTE*)raw_addr;
	EXT_SERIES(ser);

	VAL_STRUCT_DATA_BIN(out) = ser;
}
#endif

static REBOOL parse_field_type(REBSTU *stu, REBSTF *field, REBVAL *spec)
{
	REBVAL *val = VAL_BLK_DATA(spec);

	if (IS_WORD(val)){
		switch (VAL_WORD_CANON(val)) {
			case SYM_UINT8X:
				field->type = STRUCT_TYPE_UINT8;
				field->size = 1;
				break;
			case SYM_INT8X:
				field->type = STRUCT_TYPE_INT8;
				field->size = 1;
				break;
			case SYM_UINT16X:
				field->type = STRUCT_TYPE_UINT16;
				field->size = 2;
				break;
			case SYM_INT16X:
				field->type = STRUCT_TYPE_INT16;
				field->size = 2;
				break;
			case SYM_UINT32X:
				field->type = STRUCT_TYPE_UINT32;
				field->size = 4;
				break;
			case SYM_INT32X:
				field->type = STRUCT_TYPE_INT32;
				field->size = 4;
				break;
			case SYM_UINT64X:
				field->type = STRUCT_TYPE_UINT64;
				field->size = 8;
				break;
			case SYM_INT64X:
				field->type = STRUCT_TYPE_INT64;
				field->size = 8;
				break;
			case SYM_FLOATX:
				field->type = STRUCT_TYPE_FLOAT;
				field->size = 4;
				break;
			case SYM_DOUBLEX:
				field->type = STRUCT_TYPE_DOUBLE;
				field->size = 8;
				break;
			case SYM_STRUCT_TYPE:
				field->type = STRUCT_TYPE_STRUCT;
				field->size = 0;
				++ val;
				if (IS_BLOCK(val) || IS_WORD(val) || IS_INTEGER(val)) {
					REBFLG res;
					DS_PUSH_END;
					REBVAL *inner = DS_TOP;
					res = MT_Struct(inner, val, REB_STRUCT);
					if (!res) {
						//RL_Print("Failed to make nested struct!\n");
						return FALSE;
					}
					field->size = VAL_STRUCT_SIZE(inner);
					field->spec = VAL_STRUCT_SPEC(inner);
					STRUCT_FLAGS(stu) |= VAL_STRUCT_FLAGS(inner);
					DS_POP;
				}
				else {
					return FALSE;
				}
				break;
#ifdef TODO
			case SYM_POINTER:
				field->type = STRUCT_TYPE_POINTER;
				field->size = sizeof(void *);
				break;
#endif
			case SYM_WORD_TYPE:
				field->type = STRUCT_TYPE_WORD;
				field->size = 4;
				break;
			case SYM_REBVALX:
				field->type = STRUCT_TYPE_REBVAL;
				field->size = sizeof(REBVAL);
				break;
			default:
				return FALSE;
		}
	}
	else {
		return FALSE;
	}
	++ val;

	if (IS_BLOCK(val)) {// make struct [a [int32! [2]]]
		// Multi-dimensional field
		if (!IS_INTEGER(VAL_BLK_DATA(val))) {
			return FALSE;
		}
		field->dimension = (REBCNT)VAL_INT64(VAL_BLK_DATA(val));
		field->array = TRUE;
		++ val;
	} else {
		field->dimension = 1; /* scalar */
		field->array = FALSE;
	}

	if (NOT_END(val)) {
		return FALSE;
	}
	return TRUE;
}

/***********************************************************************
**
*/	REBFLG MT_Struct(REBVAL *out, REBVAL *data, REBCNT type)
/*
 * Format:
 * make struct! [
 *     field1  [type]    "Single value"
 * 	   field2  [type[3]] "Array with 3 values"
 * 	   ...
 * ]
***********************************************************************/
{
	REBVAL key;
	REBVAL spec;
	REBVAL *struct_specs = Get_System(SYS_CATALOG, CAT_STRUCTS);
	REBCNT hash = 0, n = NOT_FOUND;
	REBVAL *values = data + 1;
	REBOOL new_spec = FALSE;

	if (IS_INTEGER(data)) {
		// Struct spec id used like:
		// #(struct! 111111 [a: 1 b: 2])
		hash = VAL_INT64(data);
	}
	else if (IS_WORD(data)) {
		// Struct spec as a registered struct name
		// #(struct! some-struct [a: 1 b: 2])
		n = Find_Entry(VAL_SERIES(struct_specs), data, 0, TRUE);
	}
	else if (!IS_BLOCK(data)) return FALSE; // validate early!
	else hash = Hash_Block_Value(data);

	if (hash) {
		SET_INTEGER(&key, hash);
		n = Find_Entry(VAL_SERIES(struct_specs), &key, 0, TRUE);
	}
	if (n == NOT_FOUND) {
		if (!IS_BLOCK(data)) {
			Trap_Arg(data);
		}
		Set_Block(&spec, Clone_Block(VAL_SERIES(data)));
		LABEL_SERIES(VAL_SERIES(&spec), "struct_spec");
		// make sure that user cannot modify it
		Protect_Series(&spec, FLAGIT(PROT_SET) | FLAGIT(PROT_LOCK) | FLAGIT(PROT_DEEP));
		new_spec = TRUE;
	}
	else {
		spec = *VAL_BLK_SKIP(struct_specs, n);
	}

	REBSTU *stu = &VAL_STRUCT(out);
	VAL_STRUCT_SPEC(out) = VAL_SERIES(&spec);
	VAL_STRUCT_DATA(out) = NULL;
	//VAL_STRUCT_FIELDS(out) = NULL;
	VAL_STRUCT_OFFSET(out) = 0;
	/* set type early such that GC will handle it correctly, i.e, not collect series in the struct */
	SET_TYPE(out, REB_STRUCT);

	if (VAL_STRUCT_FIELDS(out) == NULL) {
		REBVAL *blk = VAL_BLK_DATA(&spec);
		REBINT field_num = 0; /* for field index */
		REBU64 offset = 0;    /* offset in data */
		REBCNT error = 0;

		// Count fields and validate spec to optimize series preallocation
		while (IS_STRING(blk)) ++blk;
		if (IS_BLOCK(blk)) ++blk;
		while (NOT_END(blk)) {
			if (IS_WORD(blk) && IS_BLOCK(blk + 1)) {
				++field_num;
				blk += 2;
				while (IS_STRING(blk)) ++blk;
			}
			else Trap1(RE_MALCONSTRUCT, blk);
		}
		// Don't allow empty struct!
		if (field_num == 0) Trap1(RE_MALCONSTRUCT, data);
		// Reset specification
		blk = VAL_BLK_DATA(&spec);
		// Initialize series to store field metadata
		VAL_STRUCT_FIELDS(out) = Make_Series(field_num+1, sizeof(REBSTF), FALSE); // keeps info at its head
		VAL_STRUCT_COUNT(out) = field_num;
		BARE_SERIES(VAL_STRUCT_FIELDS(out));                  // does not hold Rebol values
		KEEP_SERIES(VAL_STRUCT_FIELDS(out), "struct_fields"); // protect from GC
		SERIES_TAIL(VAL_STRUCT_FIELDS(out)) = field_num = 1;  // info at the head
		VAL_STRUCT_ID(out) = hash;

		// skip optional doc strings
		while (IS_STRING(blk)) ++blk;

		// optional attributes
		if (IS_BLOCK(blk)) {
			//TODO: ignored now!
		//	parse_attr(blk, &raw_size, &raw_addr);
			++blk;
		}

		//!!! IMPORTANT NOTE !!!
		//!!! Don't throw an error from this loop!
		//!!! VAL_STRUCT_FIELDS(out) must be manually freed if an error occurs!
		#define FIELD_ERROR_SIZE_LIMIT   1
		#define FIELD_ERROR_INVALID_SPEC 2
		while (NOT_END(blk)) {
			REBSTF *field = NULL;
			REBU64 step = 0;

			// The specification was partially verified.
			// Content should be: [WORD BLOCK opt STR...]

			field = (REBSTF *)SERIES_SKIP(VAL_STRUCT_FIELDS(out), field_num);
			field->offset = (REBCNT)offset;
			field->sym = VAL_WORD_SYM(blk);
			VAL_SET_LINE(blk);
			++blk;

			if (!parse_field_type(stu, field, blk)) {
				error = FIELD_ERROR_INVALID_SPEC;
				break;
			}
			VAL_CLR_LINE(blk);
			++blk;

			// skip optional doc strings
			while (IS_STRING(blk)) {
				VAL_CLR_LINE(blk);
				++blk;
			}

			STATIC_ASSERT(sizeof(field->size) <= 4);
			STATIC_ASSERT(sizeof(field->dimension) <= 4);

			step = (REBU64)field->size * (REBU64)field->dimension;
			if (step > VAL_STRUCT_LIMIT) {
				error = FIELD_ERROR_SIZE_LIMIT;
				break;
			}

			offset += step;
			/*
			if (alignment != 0) {
				//offset = ((offset + alignment - 1) / alignment) * alignment;
				// if alignement is power of 2:
				offset = (offset + alignment - 1) & ~(alignment - 1);
			}
			*/
			if (offset > VAL_STRUCT_LIMIT) {
				error = FIELD_ERROR_SIZE_LIMIT;
				break;
			}
			
			if (field->type == STRUCT_TYPE_REBVAL) VAL_STRUCT_FLAGS(out) |= 3; // readonly, needs GC mark
			else if (field->type == STRUCT_TYPE_STRUCT) VAL_STRUCT_FLAGS(out) |= 1; // needs GC mark
		
			field->done = TRUE;
			++SERIES_TAIL(VAL_STRUCT_FIELDS(out));
			++field_num;
		}

		if (error) {
			Free_Series(VAL_STRUCT_FIELDS(out));
			SET_UNSET(out);
			switch (error) {
			case FIELD_ERROR_SIZE_LIMIT:   Trap1(RE_SIZE_LIMIT, out);
			case FIELD_ERROR_INVALID_SPEC: Trap_Arg(blk);
			}
		}

		//Dump_Series(VAL_STRUCT_FIELDS(out), "struct_fields");

		// Store complete length of the struct
		STRUCT_SIZE(stu) = (REBCNT)offset;

		// Append value to system/catalog/structs
		n = Find_Entry(VAL_SERIES(struct_specs), &key, &spec, TRUE);
		ASSERT1(n != NOT_FOUND, RP_NO_STRUCT_REGISTER);
	}

	// At this point, the struct specification should be ready.

	if (IS_BINARY(values)) {
		if (VAL_BIN_LEN(values) < STRUCT_SIZE(stu)) Trap_Arg(values);
		if (STRUCT_DATA(stu)) {
			COPY_MEM(STRUCT_DATA_BIN(stu), VAL_BIN_DATA(values), STRUCT_SIZE(stu));
		}
		else {
			STRUCT_DATA(stu) = VAL_SERIES(values);
		}
		return TRUE;
	}
	
	STRUCT_DATA(stu) = Make_Binary(STRUCT_SIZE(stu));
	LABEL_SERIES(VAL_STRUCT_FIELDS(out), "struct_data");
	SERIES_TAIL(STRUCT_DATA(stu)) = STRUCT_SIZE(stu);
	
	if (IS_BLOCK(values)) {
		init_fields(out, values);
	}
	return TRUE;
}


/***********************************************************************
**
*/	REBINT PD_Struct(REBPVS *pvs)
/*
***********************************************************************/
{
	REBSTU *stu = &VAL_STRUCT(pvs->value);
	if (!IS_WORD(pvs->select)) {
		return PE_BAD_SELECT;
	}
	if (! pvs->setval || NOT_END(pvs->path + 1)) {
		if (!Get_Struct_Var(stu, pvs->select, pvs->store)) {
			return PE_BAD_SELECT;
		}

		/* Setting element to an array in the struct:
		 * struct/field/1: 0
		 * */
		if (pvs->setval
			&& IS_BLOCK(pvs->store)
			&& IS_END(pvs->path + 2)) {
			REBVAL *sel = pvs->select;
			pvs->value = pvs->store;
			Next_Path(pvs); // sets value in pvs->value
			if (!Set_Struct_Var(stu, sel, pvs->select, pvs->value)) {
				return PE_BAD_SET;
			}
			return PE_OK;
		}
		return PE_USE;
	} else {// setval && END
		if (!Set_Struct_Var(stu, pvs->select, NULL, pvs->setval)) {
			return PE_BAD_SET;
		}
		return PE_OK;
	}
	return PE_BAD_SELECT;
}

/***********************************************************************
**
*/	REBINT Cmp_Struct(REBVAL *s, REBVAL *t)
/*
***********************************************************************/
{
	REBINT n = VAL_STRUCT_FIELDS(s) - VAL_STRUCT_FIELDS(t);
	if (n != 0) {
		return n;
	}
	n = VAL_STRUCT_DATA(s) - VAL_STRUCT_DATA(t);
	return n;
}

/***********************************************************************
**
*/	REBINT CT_Struct(REBVAL *a, REBVAL *b, REBINT mode)
/*
***********************************************************************/
{
	//printf("comparing struct a (%p) with b (%p), mode: %d\n", a, b, mode);
	switch (mode) {
		case 3: /* same? */
		case 2: /* strict equality */
			return 0 == Cmp_Struct(a, b);
		case 1: /* equvilance */
		case 0: /* coersed equality*/
			if (Cmp_Struct(a, b) == 0) {
				return 1;
			}
			return IS_STRUCT(a) && IS_STRUCT(b)
				 && same_fields(VAL_STRUCT_FIELDS(a), VAL_STRUCT_FIELDS(b))
				 && VAL_STRUCT_SIZE(a) == VAL_STRUCT_SIZE(b)
				 && !memcmp(VAL_STRUCT_DATA_BIN(a), VAL_STRUCT_DATA_BIN(b), VAL_STRUCT_SIZE(a));
		default:
			return -1;
	}
	return -1;
}

static void Copy_Struct(REBSTU *src, REBSTU *dst)
{
	/* Read only field */
	dst->spec = src->spec;

	/* Writable field */
	STRUCT_DATA(dst) = Make_Binary(STRUCT_SIZE(src));
	BARE_SERIES(STRUCT_DATA(dst));
	SERIES_TAIL(STRUCT_DATA(dst)) = STRUCT_SIZE(src);
	COPY_MEM(STRUCT_DATA_BIN(dst), STRUCT_DATA_BIN(src), STRUCT_SIZE(src));
}

static void Copy_Struct_Val(REBVAL *src, REBVAL *dst)
{
	SET_STRUCT(dst);
	Copy_Struct(&VAL_STRUCT(src), &VAL_STRUCT(dst));
}

static void init_field(REBVAL *ret, REBSTF *fld, REBVAL *value) {
	if (fld->dimension > 1) {
		if (IS_BLOCK(value)) {
			if (VAL_LEN(value) != fld->dimension) {
				Trap_Arg(value);
			}
			for (REBCNT n = 0; n < fld->dimension; n++) {
				if (!assign_scalar(&VAL_STRUCT(ret), fld, n, VAL_BLK_SKIP(value, n))) {
					Trap_Arg(value);
				}
			}
		}
		else {
			Trap_Arg(value);
		}
	}
	else {
		if (!assign_scalar(&VAL_STRUCT(ret), fld, 0, value)) {
			Trap_Arg(value);
		}
	}
}

/* a: make struct! [i: [uint8!] 1]
 * b: make a [i: 10]
 */
static void init_fields(REBVAL *ret, REBVAL *spec)
{
	REBVAL *blk = VAL_BLK_DATA(spec);
	REBVAL *word;
	REBVAL *fld_val;
	REBSTF *fld = NULL;
	REBSER *fields = VAL_STRUCT_FIELDS(ret);

	if (IS_SET_WORD(blk)) {
		// Specification in format like: [a: 1 b: 2]
		for (; NOT_END(blk); blk += 2) {
			REBCNT i = 0;
			word = blk;
			fld_val = blk + 1;

			if (IS_END(fld_val)) {
				Trap1(RE_NEED_VALUE, word);
			}
			// Iterate all fields (first value is used for info)
			for (i = 1; i < SERIES_TAIL(fields); i++) {
				fld = (REBSTF *)SERIES_SKIP(fields, i);
				if (fld->sym == VAL_WORD_CANON(word)) {
					init_field(ret, fld, fld_val);
					break;
				}
			}

			if (i == SERIES_TAIL(fields)) {
				Trap_Arg(word); /* field not found in the parent struct */
			}
		}
	}
	else {
		// Only values...
		for (REBCNT i = 1; i < SERIES_TAIL(fields); i++) {
			if (IS_END(blk)) return;
			fld = (REBSTF *)SERIES_SKIP(fields, i);
			init_field(ret, fld, blk);
			++blk;
		}
	}
}

/***********************************************************************
**
*/	REBTYPE(Struct)
/*
***********************************************************************/
{
	REBVAL *val;
	REBVAL *arg;
	REBSTU *strut;
	REBVAL *ret;
	
	arg = D_ARG(2);
	val = D_ARG(1);
	strut = 0;

	ret = DS_RETURN;
	// unary actions
	switch(action) {
		case A_MAKE:
		case A_TO:
			// Clone an existing STRUCT:
			if (IS_STRUCT(val)) {
				Copy_Struct_Val(val, ret);
				/* only accept value initialization */
				if (IS_BLOCK(arg)) {
					Reduce_Block_No_Set(VAL_SERIES(arg), VAL_INDEX(arg), NULL);
					init_fields(ret, DS_TOP);
				}
				else if (IS_BINARY(arg) && VAL_BIN_LEN(arg) >= VAL_STRUCT_SIZE(val)) {
					//TODO: special error when data are not large enough?
					COPY_MEM(VAL_STRUCT_DATA_BIN(ret), VAL_BIN_DATA(arg), VAL_STRUCT_SIZE(val));
				}
				else {
					Trap_Arg(arg);
				}
			} else if (!IS_DATATYPE(val)) {
				goto is_arg_error;
			} else {
				// Initialize STRUCT from block:
				// make struct! [a [uint16!]]
				if (IS_BLOCK(arg)) {
					DS_PUSH_END;
					if (!MT_Struct(ret, arg, REB_STRUCT)) {
						goto is_arg_error;
					}
				} else {
					Trap_Make(REB_STRUCT, arg);
				}
			}
			SET_TYPE(ret, REB_STRUCT);
			break;

		case A_CHANGE:
			{
				if (IS_BLOCK(arg)) {
					init_fields(val, arg);
					return R_ARG1;
				}
				if (VAL_STRUCT_PROTECTED(val)) Trap0(RE_PROTECTED);
				if (!IS_BINARY(arg)) {
					Trap_Types(RE_EXPECT_VAL, REB_BINARY, VAL_TYPE(arg));
				}
				COPY_MEM(VAL_STRUCT_DATA_BIN(val), VAL_BIN_DATA(arg), min(VAL_BIN_LEN(arg),VAL_STRUCT_SIZE(val)));
				return R_ARG1;
			}
			break;
		case A_REFLECT:
			{
				REBINT n = VAL_WORD_CANON(arg); // zero on error
				switch (n) {
					case SYM_WORDS:
					case SYM_VALUES:
					case SYM_BODY:
						Get_Struct_Reflect(ret, &VAL_STRUCT(val), n);
						break;
					case SYM_SPEC:
						// no need to copy as it is protected value
						Set_Block(ret, VAL_STRUCT_SPEC(val));
						break;
					default:
						Trap_Reflect(REB_STRUCT, arg);
				}
			}
			break;
		//TODO: A_QUERY to access struct's name and id?

		case A_LENGTHQ:
			SET_INTEGER(ret, VAL_STRUCT_SIZE(val));
			break;

		case A_CLEAR:
			CLEAR(VAL_STRUCT_DATA_BIN(val), VAL_STRUCT_SIZE(val))
			return R_ARG1;

		case A_COPY:
			// Allow only a simple copy without any refinements.
			if (D_REF(ARG_COPY_PART) || D_REF(ARG_COPY_DEEP) || D_REF(ARG_COPY_TYPES))
				Trap0(RE_BAD_REFINES);
			Copy_Struct_Val(val, ret);
			break;

		default:
			Trap_Action(REB_STRUCT, action);
	}
	return R_RET;

is_arg_error:
	Trap_Types(RE_EXPECT_VAL, REB_STRUCT, VAL_TYPE(arg));
}
