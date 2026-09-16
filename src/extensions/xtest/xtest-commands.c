//
// Project: Rebol/XTest extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Command implementations for the extension interface test module.
//
// One function per command; the enum, the declarations, the dispatch table
// and the `_init` handler are all generated from xtest.reb.
//

#include "gen-xtest.h"
#include "xtest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARG_Is_XTest(n)  FRM_IS_HANDLE(n, Handle_XTest)

static const REBYTE* ERR_INVALID_HANDLE = (const REBYTE*)"Invalid XTest handle!";
static const REBYTE* ERR_NO_HANDLE      = (const REBYTE*)"Failed to create the XTest handle!";
static const REBYTE* ERR_BAD_STRUCT     = (const REBYTE*)"Unusable struct argument!";
static const REBYTE* ERR_RAW_STRUCT     = (const REBYTE*)"Struct does not allow raw modification!";
static const REBYTE* ERR_NO_FIELD       = (const REBYTE*)"No such field in the struct!";
static const REBYTE* ERR_NOT_ARRAY      = (const REBYTE*)"The field is not an array!";
static const REBYTE* ERR_NOT_INT_ARRAY  = (const REBYTE*)"The field is not an array of integers!";


//== callbacks ================================================================

REBCNT Test_Sync_Callback(REBSER *obj, REBCNT word, RXIARG *result) {
	RXICBI cbi;
	RXIARG args[4];
	REBCNT n;

	// These can live on the stack, because the call is synchronous.
	CLEAR(&cbi, sizeof(cbi));
	CLEAR(&args[0], sizeof(args));
	cbi.obj  = obj;
	cbi.word = word;
	cbi.args = args;

	// Pass a single integer arg to the callback function:
	RXI_COUNT(args) = 1;
	RXI_TYPE(args, 1) = RXT_INTEGER;
	args[1].int64 = 123;

	n = RL_CALLBACK(&cbi);

	*result = cbi.result;
	return n;
}

REBCNT Test_Async_Callback(REBSER *obj, REBCNT word) {
	RXICBI *cbi;
	RXIARG *args;
	REBCNT n;

	// These cannot live on the stack - they are used later, when the
	// callback event is finally processed.
	cbi  = MAKE_CLEAR_MEM(sizeof(RXICBI));
	args = MAKE_CLEAR_MEM(sizeof(RXIARG) * 4);
	// Freed in do_callback (f-extension.c) when RXC_ALLOC is set.

	if (!cbi || !args) return 0;
	cbi->obj  = obj;
	cbi->word = word;
	cbi->args = args;
	SET_FLAG(cbi->flags, RXC_ASYNC);
	SET_FLAG(cbi->flags, RXC_ALLOC);

	RXI_COUNT(args) = 1;
	RXI_TYPE(args, 1) = RXT_INTEGER;
	args[1].int64 = 1234;

	n = RL_CALLBACK(cbi); // result is in the cbi struct, if wanted
	return n;
}


//== argument passing =========================================================

COMMAND cmd_xtest_xarg0(RXIFRM *frm, void *ctx) {
	RXA_INT64(frm, 1) = 0;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

COMMAND cmd_xtest_xarg1(RXIFRM *frm, void *ctx) {
	// returns its argument unchanged
	return RXR_VALUE;
}

COMMAND cmd_xtest_xarg2(RXIFRM *frm, void *ctx) {
	RXA_ARG(frm, 1)  = RXA_ARG(frm, 2);
	RXA_TYPE(frm, 1) = RXA_TYPE(frm, 2);
	return RXR_VALUE;
}

COMMAND cmd_xtest_echo(RXIFRM *frm, void *ctx) {
	return RXR_VALUE;
}


//== words and objects ========================================================

COMMAND cmd_xtest_xword0(RXIFRM *frm, void *ctx) {
	RXA_WORD(frm, 1) = AS_WORD("system");
	RXA_TYPE(frm, 1) = RXT_WORD;
	return RXR_VALUE;
}

COMMAND cmd_xtest_xword1(RXIFRM *frm, void *ctx) {
	REBYTE *str = NULL;
	RL_GET_STRING(RXA_SERIES(frm, 1), 0, (void*)(&str), FALSE);
	RXA_WORD(frm, 1) = RL_MAP_WORD(str);
	RXA_TYPE(frm, 1) = RXT_WORD;
	return RXR_VALUE;
}

COMMAND cmd_xtest_xobj1(RXIFRM *frm, void *ctx) {
	RXA_TYPE(frm, 1) = RL_GET_FIELD(RXA_OBJECT(frm, 1), RXA_WORD(frm, 2), &RXA_ARG(frm, 1));
	return RXR_VALUE;
}

COMMAND cmd_xtest_xobj2(RXIFRM *frm, void *ctx) {
	REBSER *obj   = RXA_OBJECT(frm, 1);
	REBCNT *words = RL_WORDS_OF_OBJECT(obj);
	REBCNT type, index;
	RXIARG val;

	// one less, because the first value is the length!
	printf("Object has %u fields:\n", words[0] - 1);
	for (size_t i = 1; words[i] != 0; i++) {
		REBYTE *name = RL_WORD_STRING(words[i]);
		index = RL_FIND_WORD(words, words[i]);
		type  = RL_GET_FIELD(obj, words[i], &val);
		printf(" field %u\ttype: %u\tname: %s\n", index, type, name);
		free(name); // release the name when not needed anymore!
	}
	free(words);

	RXA_TYPE(frm, 1) = RXT_UNSET;
	return RXR_VALUE;
}


//== callbacks ================================================================

COMMAND cmd_xtest_calls(RXIFRM *frm, void *ctx) {
	RXA_TYPE(frm, 1) = Test_Sync_Callback(RXA_OBJECT(frm, 1), RXA_WORD(frm, 2), &RXA_ARG(frm, 1));
	return RXR_VALUE;
}

COMMAND cmd_xtest_calla(RXIFRM *frm, void *ctx) {
	RXA_LOGIC(frm, 1) = Test_Async_Callback(RXA_OBJECT(frm, 1), RXA_WORD(frm, 2));
	RXA_TYPE(frm, 1) = RXT_LOGIC;
	return RXR_VALUE;
}


//== images ===================================================================

COMMAND cmd_xtest_img0(RXIFRM *frm, void *ctx) {
	RXA_TYPE(frm, 1) = RXT_IMAGE;
	RXA_IMAGE(frm, 1) = RL_MAKE_IMAGE(2, 3);
	RXA_IMAGE_WIDTH(frm, 1) = 2;
	RXA_IMAGE_HEIGHT(frm, 1) = 3;
	return RXR_VALUE;
}


//== command context ==========================================================

COMMAND cmd_xtest_cec0(RXIFRM *frm, void *ctx) {
	REBCEC cec;
	cec.envr  = 0;
	cec.block = RXA_SERIES(frm, 1);
	cec.index = 0;
	RL_DO_COMMANDS(RXA_SERIES(frm, 1), 0, &cec);
	return RXR_UNSET;
}

COMMAND cmd_xtest_cec1(RXIFRM *frm, void *ctx) {
	REBCEC *cec = (REBCEC*)ctx;
	RXA_INT64(frm, 1) = (i64)(cec ? cec->index : -1);
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}


//== plain handles ============================================================

COMMAND cmd_xtest_hndl1(RXIFRM *frm, void *ctx) {
	RXA_HANDLE(frm, 1) = (void*)42;
	RXA_HANDLE_TYPE(frm, 1) = AS_WORD("xtest_plain");
	RXA_TYPE(frm, 1) = RXT_HANDLE;
	return RXR_VALUE;
}

COMMAND cmd_xtest_hndl2(RXIFRM *frm, void *ctx) {
	i64 i = (i64)RXA_HANDLE(frm, 1);
	RXA_INT64(frm, 1) = i;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}


//== vectors ==================================================================

COMMAND cmd_xtest_vec0(RXIFRM *frm, void *ctx) {
	REBSER *vec  = RXA_VECTOR_SERIES(frm, 1);
	REBCNT  info = RXA_VECTOR_INFO(frm, 1);
	REBCNT  bits = RXI_VECTOR_BITS(info);
	REBCNT  tail = (REBCNT)RL_SERIES(vec, RXI_SER_TAIL);

	RXA_TYPE(frm, 1) = RXT_INTEGER;
	RXA_INT64(frm, 1) = (bits / 8) * tail;
	return RXR_VALUE;
}

COMMAND cmd_xtest_vec1(RXIFRM *frm, void *ctx) {
	RXIARG vec;
	REBCNT type = RL_GET_FIELD(RXA_OBJECT(frm, 1), AS_WORD("v"), &vec);

	if (type != RXT_VECTOR) return RXR_FALSE;
	{
		REBSER *vecs = vec.vector.series;
		REBCNT  info = vec.vector.info;
		REBCNT  tail = (REBCNT)RL_SERIES(vecs, RXI_SER_TAIL);
		REBCNT  rows = RXI_VECTOR_ROWS(info);
		REBCNT  cols = RXI_VECTOR_COLS(tail, info);

		// The whole point of this test is that the packed info survives
		// the crossing, so report it instead of peeking at raw bytes
		// through a hardcoded element width.
		printf("vector: %u-bit %s%s  shape: %ux%u\n",
			RXI_VECTOR_BITS(info),
			RXI_VECTOR_SIGNED(info) ? "signed" : "unsigned",
			RXI_VECTOR_FLOAT(info)  ? " float" : "",
			cols, rows);

		RXA_TYPE(frm, 1) = RXT_INTEGER;
		RXA_INT64(frm, 1) = (i64)(rows * cols);
	}
	return RXR_VALUE;
}


//== blocks ===================================================================

COMMAND cmd_xtest_blk1(RXIFRM *frm, void *ctx) {
	REBSER *blk = RXA_SERIES(frm, 1);
	REBCNT n, type;
	RXIARG val;

	printf("\nBlock with %u values:\n", (REBLEN)RL_SERIES(blk, RXI_SER_TAIL));
	for (n = 0; (type = RL_GET_VALUE(blk, n, &val)); n++) {
		if (type == RXT_END) break;
		printf("\t%i -> %i\n", n, type);
	}
	RL_MAP_WORDS(RXA_SERIES(frm, 1));
	return RXR_UNSET;
}


//== context handles ==========================================================

COMMAND cmd_xtest_hob1(RXIFRM *frm, void *ctx) {
	REBHOB *hob = RL_MAKE_HANDLE_CONTEXT(Handle_XTest);
	REBSER *bin = RXA_SERIES(frm, 1);
	XTEST  *data;

	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);
	data = (XTEST*)hob->data;

	if (SERIES_REST(bin) < 1) {
		RL_EXPAND_SERIES(bin, SERIES_TAIL(bin), 1);
	}

	if (RXA_REF(frm, 2)) {
		hob->series = RL_MAKE_BLOCK(2);
		data->flags = 1;
		RL_SET_VALUE(hob->series, 0, RXA_ARG(frm, 1), RXT_BINARY);
		RL_SET_VALUE(hob->series, 1, RXA_ARG(frm, 3), RXT_HANDLE);
	} else {
		hob->series = bin;
		data->flags = 0;
	}

	printf("data=> id: %u flags: %i\n", data->id, data->flags);
	data->id = 1;
	printf("data=> id: %u flags: %i\n", data->id, data->flags);

	RXA_HANDLE(frm, 1) = hob;
	RXA_HANDLE_TYPE(frm, 1) = hob->sym;
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;
	RXA_TYPE(frm, 1) = RXT_HANDLE;
	return RXR_VALUE;
}

COMMAND cmd_xtest_hob2(RXIFRM *frm, void *ctx) {
	REBHOB *hob = RXA_HANDLE(frm, 1);

	if (!ARG_Is_XTest(1)) RETURN_ERROR(ERR_INVALID_HANDLE);
	{
		XTEST *data = (XTEST*)hob->data;
		REBSER *bin;
		REBCNT type;

		if (data->flags == 1) {
			type = RL_GET_VALUE(hob->series, 0, &RXA_ARG(frm, 2));
			if (type != RXT_BINARY) return RXR_FALSE;
			bin = RXA_SERIES(frm, 2);
		} else {
			bin = hob->series;
		}

		SERIES_DATA(bin)[0] = SERIES_DATA(bin)[0] + 1;
		printf("data=> id: %u flags: %i b: %i\n", data->id, data->flags, (u8)SERIES_DATA(bin)[0]);
		RXA_INT64(frm, 1) = SERIES_DATA(bin)[0];
		RXA_TYPE(frm, 1) = RXT_INTEGER;
	}
	return RXR_VALUE;
}


//== strings and paths ========================================================

COMMAND cmd_xtest_str0(RXIFRM *frm, void *ctx) {
	REBSER *str = RL_MAKE_STRING(32, FALSE); // 32 bytes, latin1 (must be large enough!)
	REBYTE ver[8];

	RL_VERSION(ver);
	snprintf(s_cast(SERIES_DATA(str)), SERIES_REST(str), "Version: %i.%i.%i", ver[1], ver[2], ver[3]);
	SERIES_TAIL(str) = LEN_BYTES(SERIES_DATA(str));

	RXA_SERIES(frm, 1) = str;
	RXA_TYPE(frm, 1) = RXT_STRING;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}

COMMAND cmd_xtest_path(RXIFRM *frm, void *ctx) {
	// The input series is now always encoded as UTF8.
	REBSER *ser = RL_TO_LOCAL_PATH(&RXA_ARG(frm, 1), RXA_REF(frm, 2), 1);
	RXA_SERIES(frm, 1) = ser;
	RXA_TYPE(frm, 1) = RXT_STRING;
	RXA_INDEX(frm, 1) = 0;
	return RXR_VALUE;
}


//== structs ==================================================================

COMMAND cmd_xtest_stru(RXIFRM *frm, void *ctx) {
	RXISTRU stru;
	REBSTF *field;
	REBYTE *word;

	// One lookup gives the bytes of THIS struct, its real size and its flags.
	// It also rejects a view which does not fit into the data series.
	if (!RXA_STRUCT_INFO(frm, 1, &stru)) RETURN_ERROR(ERR_BAD_STRUCT);

	printf("struct id: %u size: %u fields: %u flags: %u offset: %u\n",
		stru.id, stru.size, stru.count, stru.flags,
		RXA_STRUCT_OFFSET(frm, 1));

	field = RXI_STRUCT_FIELDS(&stru);
	for (REBCNT i = 0; i < stru.count; ++i, ++field) {
		word = RL_WORD_STRING(field->sym); // allocates a new string!
		printf(" field name: %s\n", word);
		printf("       type: %i size: %u offset: %u dim: %u total: %u\n",
			field->type, field->size, field->offset,
			field->dimension, field->size * field->dimension);
		free(word); // release the string
	}

	// Inspection is always safe, so /read stops here.
	if (RXA_REF(frm, 2)) return RXR_VALUE;

	// Writing raw bytes is not: MARK data holds real REBVALs which the GC
	// walks, and PROTECTED forbids raw modification. Refuse loudly rather
	// than corrupt the data - this is the guard every extension touching
	// struct bytes must make.
	if (!RXI_STRUCT_WRITABLE(&stru)) RETURN_ERROR(ERR_RAW_STRUCT);

	if (stru.size > 0) stru.data[0] = stru.data[0] + 1;

	return RXR_VALUE;
}

COMMAND cmd_xtest_stru0(RXIFRM *frm, void *ctx) {
	RXISTRU stru;
	// Read the id BEFORE overwriting the argument slot with the new struct.
	REBCNT id = RXA_STRUCT_ID(frm, 1);

	if (!RL_MAKE_STRUCT(&RXA_ARG(frm, 1), id, &stru))
		RETURN_ERROR(ERR_BAD_STRUCT);

	printf("new struct id: %u size: %u fields: %u flags: %u\n",
		stru.id, stru.size, stru.count, stru.flags);

	// Nothing to initialize: the data are zeroed, which is also what makes
	// a struct with `rebval!` fields safe to hand back to the GC.
	RXA_TYPE(frm, 1) = RXT_STRUCT;
	return RXR_VALUE;
}

// Sum the elements of an integer array field, addressed the way an
// extension must: base + field->offset + n * field->size. The data are
// packed, so an element may be misaligned and has to be copied out - never
// read through a typed pointer (undefined behavior).
#define SUM_ELEMENT(type) { type v; COPY_MEM(&v, el, sizeof(v)); sum += (i64)v; break; }

COMMAND cmd_xtest_strua(RXIFRM *frm, void *ctx) {
	RXISTRU stru;
	REBSTF *field;
	REBCNT  sym = RXA_WORD(frm, 2); // canon symbol, same id as field->sym
	REBCNT  i, n;
	i64     sum = 0;

	if (!RXA_STRUCT_INFO(frm, 1, &stru)) RETURN_ERROR(ERR_BAD_STRUCT);

	field = RXI_STRUCT_FIELDS(&stru);
	for (i = 0; i < stru.count; ++i, ++field) {
		if (field->sym == sym) break;
	}
	if (i >= stru.count) RETURN_ERROR(ERR_NO_FIELD);
	if (!field->array)   RETURN_ERROR(ERR_NOT_ARRAY);

	printf("array field: type: %i size: %u dim: %u offset: %u total: %u\n",
		field->type, field->size, field->dimension, field->offset,
		field->size * field->dimension);

	for (n = 0; n < field->dimension; ++n) {
		REBYTE *el = stru.data + field->offset + n * field->size;
		switch (field->type) {
		case STRUCT_TYPE_UINT8:  SUM_ELEMENT(u8)
		case STRUCT_TYPE_INT8:   SUM_ELEMENT(i8)
		case STRUCT_TYPE_UINT16: SUM_ELEMENT(u16)
		case STRUCT_TYPE_INT16:  SUM_ELEMENT(i16)
		case STRUCT_TYPE_UINT32: SUM_ELEMENT(u32)
		case STRUCT_TYPE_INT32:  SUM_ELEMENT(i32)
		case STRUCT_TYPE_UINT64: SUM_ELEMENT(u64)
		case STRUCT_TYPE_INT64:  SUM_ELEMENT(i64)
		default: RETURN_ERROR(ERR_NOT_INT_ARRAY);
		}
	}

	RXA_INT64(frm, 1) = sum;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

//== handle callbacks =========================================================

int XTestContext_free(void *hndl) {
	REBHOB *hob;
	XTEST  *data;

	if (!hndl) return 0;
	hob  = (REBHOB*)hndl;
	data = (XTEST*)hob->data;
	if (!data) return 0;

	debug_print("releasing XTest handle: %p id: %u flags: %i\n", (void*)data, data->id, data->flags);
	CLEARS(data);
	UNMARK_HOB(hob);
	return 0;
}

int XTestContext_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	XTEST *xtest = (XTEST*)hob->data;
	word = RL_FIND_WORD(Xtest_arg_words, word);
	switch (word) {
	case W_XTEST_ARG_ID:
		*type = RXT_INTEGER;
		arg->int64 = xtest->id;
		break;
	case W_XTEST_ARG_DATA:
		arg->series = hob->series;
		arg->index = 0;
		*type = (xtest->flags == 1) ? RXT_BLOCK : RXT_BINARY;
		break;
	case W_XTEST_ARG_LENGTH:
		*type = RXT_INTEGER;
		arg->int64 = SERIES_TAIL(hob->series);
		break;
	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}

int XTestContext_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	XTEST *xtest = (XTEST*)hob->data;
	word = RL_FIND_WORD(Xtest_arg_words, word);
	switch (word) {
	case W_XTEST_ARG_ID:
		if (*type != RXT_INTEGER) return PE_BAD_SET_TYPE;
		xtest->id = arg->int64;
		break;
	case W_XTEST_ARG_DATA:
		if (*type != RXT_BINARY) return PE_BAD_SET_TYPE;
		hob->series = arg->series;
		break;
	default:
		return PE_BAD_SET;
	}
	return PE_OK;
}

int XTestContext_mold(REBHOB *hob, REBSER *str) {
	int len;
	XTEST *xtest;

	if (!str || !hob || !(xtest = (XTEST*)hob->data)) return 0;

	SERIES_TAIL(str) = 0;
	APPEND_STRING(str, "0#%lx id: %u", (unsigned long)(uintptr_t)hob->data, xtest->id);
	return len;
}

//== device registration ======================================================

COMMAND cmd_xtest_xdev(RXIFRM *frm, void *ctx) {
	RXA_INT64(frm, 1) = (i64)Xtest_Dev_Id;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

COMMAND cmd_xtest_xdev_poll(RXIFRM *frm, void *ctx) {
	RXA_INT64(frm, 1) = (i64)Xtest_Dev_Polls;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

// Registration must refuse a device already in the table and a caller whose
// REBDEV does not match the host's - and neither refusal may consume a slot,
// which is what the unchanged id in the test afterwards checks.
COMMAND cmd_xtest_xdev_err(RXIFRM *frm, void *ctx) {
	u32 size = (u32)RXA_INT64(frm, 1);
	if (size == 0) size = sizeof(REBDEV);
	RXA_INT64(frm, 1) = (i64)RL_REGISTER_DEVICE(&Dev_XTest, size);
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}