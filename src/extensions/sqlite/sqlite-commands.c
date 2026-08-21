//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/SQLite extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Command implementations. One function per command; the enum, the
// declarations, the dispatch table and the module init code are all
// generated from sqlite.reb.
//
// Use on your own risc!

#include "gen-sqlite.h"
#include "sqlite.h"
#include <string.h>
#include <inttypes.h>


//== helpers ==================================================================

REBSER *utf8_string(RXIARG arg) {
	REBSER *ser = arg.series;
	REBCNT  idx = arg.index;
	return RL_ENCODE_UTF8_STRING(SERIES_SKIP(ser, idx), SERIES_TAIL(ser)-idx, SERIES_WIDE(ser) > 1, FALSE);
}

int bind_parameters(sqlite3_stmt *stmt, REBSER *params, REBCNT *index) {
	REBINT count, col, rc, type ;
	RXIARG   arg = {0};
	REBSER  *ser;
	REBCNT   idx = *index;
	REBOOL   blockData = FALSE;

	rc = 0;

	// bind statement's parameters...
	if (params && stmt) {

		type = RL_GET_VALUE_RESOLVED(params, idx, &arg);
		if (type == RXT_BLOCK) {
			//trace("block params");
			params = arg.series;
			idx = arg.index;
			blockData = TRUE;
		}
		//trace("bind");
		//sqlite3_reset(stmt);
		count = sqlite3_bind_parameter_count(stmt);
		if (count) sqlite3_clear_bindings(stmt);

		//debug_print("binding %i parameters... index: %u\n", count, idx);

		for(col = 0; col < count; idx++) {
			col++;
			type = RL_GET_VALUE_RESOLVED(params, idx, &arg);
			rc = SQLITE_MISUSE;
			switch(type) {
			case RXT_INTEGER:
				rc = sqlite3_bind_int64(stmt, col, arg.int64);
				break;
			case RXT_DECIMAL:
				rc = sqlite3_bind_double(stmt, col, arg.dec64);
				break;
			case RXT_STRING:
			case RXT_FILE:
			case RXT_EMAIL:
			case RXT_REF:
			case RXT_URL:
			case RXT_TAG:
				// Make sure to convert unicode string to UTF-8
				ser = (REBSER*)arg.series;
				if (SERIES_WIDE(ser) > 1) {
					ser = RL_ENCODE_UTF8_STRING(SERIES_DATA(ser), SERIES_TAIL(ser), TRUE, FALSE);
					arg.index = 0;
				}
				rc = sqlite3_bind_text(stmt, col, SERIES_SKIP(ser, arg.index),  SERIES_TAIL(ser)-arg.index, SQLITE_TRANSIENT);
				break;
			case RXT_NONE:
				rc = sqlite3_bind_null(stmt, col);
				break;
			case RXT_LOGIC:
				rc = sqlite3_bind_int(stmt, col, arg.int32a);
				break;
			case RXT_BINARY:
				ser = (REBSER*)arg.series;
				rc = sqlite3_bind_blob(stmt, col, SERIES_SKIP(ser, arg.index), SERIES_TAIL(ser)-arg.index, SQLITE_TRANSIENT);
				break;
			case RXT_VECTOR:
				ser = arg.vector.series;
				rc = sqlite3_bind_blob(stmt, col, SERIES_DATA(ser), SERIES_TAIL(ser) * (RXI_VECTOR_BITS(arg.vector.info) / 8), SQLITE_TRANSIENT);
				break;
			case RXT_END:
				rc = SQLITE_OK;
				break;
			}
		}
	}
	//debug_print("bind rc: %i\n", rc);
	*index = blockData ? *index+1 : idx;
	return rc;
}


//== library lifecycle ========================================================

COMMAND cmd_sqlite_initialize(RXIFRM* frm, void* reb_ctx) {
	int rc  = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		snprintf(error_buffer, 254, "[SQLITE] %s", sqlite3_errstr(rc));
		RETURN_ERROR(error_buffer);
	}
	return RXR_TRUE;
}

COMMAND cmd_sqlite_shutdown(RXIFRM* frm, void* reb_ctx) {
	int rc  = sqlite3_shutdown();
	if (rc != SQLITE_OK) {
		snprintf(error_buffer, 254, "[SQLITE] %s", sqlite3_errstr(rc));
		RETURN_ERROR(error_buffer);
	}
	return RXR_TRUE;
}

COMMAND cmd_sqlite_info(RXIFRM* frm, void* reb_ctx) {
	// return library info...
	REBI64  tail = 0;
	REBSER *str  = RL_MAKE_STRING(1000, FALSE); // 1024 bytes, latin1 (must be large enough!)
	REBYTE rebol_version[8];

	if (RXT_HANDLE == RXA_TYPE(frm, 2)) {
		// Info about given handle...
		REBHOB* hob = RXA_HANDLE_CONTEXT(frm, 2);
		//debug_print("hob: %p\n", hob);

		if (hob->sym == Handle_SQLiteDB) {
			SQLITE_CONTEXT* ctx = (SQLITE_CONTEXT*)hob->data;
			//debug_print("ctx: %p\n", ctx);
			if(!ctx) return RXR_NONE;
			tail = snprintf(
				SERIES_TEXT(str),
				SERIES_REST(str),
				"sqlite-ctx-Ptr: <%p>\n"
				"sqlite-ctx-DB:  <%p>\n"
				"sqlite-buffer-size: %i\n",
				(void*)ctx,
				ctx->db,
				(ctx->buf ? SERIES_REST(ctx->buf) : 0)
			);
		}
		else if (hob->sym == Handle_SQLiteSTMT) {
			SQLITE_STMT* ctx = (SQLITE_STMT*)hob->data;
			//debug_print("ctx: %p\n", ctx);
			if(!ctx) return RXR_NONE;
			tail = snprintf(
				SERIES_TEXT(str),
				SERIES_REST(str),
				"sqlite-stmt-Ptr:  <%p>\n"
				"sqlite-stmt:      <%p>\n"
				"last-result-code:  %i\n"
				"bind-parameters:   %i\n"
				"data-count:        %i\n",
				(void*)ctx,
				ctx->stmt,
				ctx->last_result_code,
				(ctx->stmt ? sqlite3_bind_parameter_count(ctx->stmt) : 0),
				sqlite3_data_count(ctx->stmt)
			);
		}
		else {
			// unsupported handle
		}
	}
	else {
		// Some system info...
		RL_VERSION(rebol_version);

		tail = snprintf(
			SERIES_TEXT(str),
			SERIES_REST(str),
			"\n"
#if defined(MIN_REBOL_VER)
			"Rebol-needed:   %u.%u.%u\n"
#endif
			"Rebol-current:  %u.%u.%u\n"
			"SQLite-version: %s\n"
			"SQLite-memory:  %llu\n" // the number of bytes of memory currently outstanding (malloced but not freed)
			"SQLite-mem-top: %llu\n",
#if defined(MIN_REBOL_VER)
			MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD,
#endif
			rebol_version[1], rebol_version[2], rebol_version[3],
			sqlite3_libversion(),
			sqlite3_memory_used(),
			sqlite3_memory_highwater(0)
		);
	}
	if (tail < 0) return RXR_NONE;
	else {
		SERIES_TAIL(str) = (REBCNT)tail;
		RXA_SERIES(frm, 1) = str;
		RXA_TYPE(frm, 1) = RXT_STRING;
		RXA_INDEX(frm, 1) = 0;
	}
	return RXR_VALUE;
}


//== connections ==============================================================

COMMAND cmd_sqlite_open(RXIFRM* frm, void* reb_ctx) {
	REBSER  *filename;
	REBHOB  *hob;
	SQLITE_CONTEXT *ctx;
	int rc;

	filename = utf8_string(RXA_ARG(frm, 1));

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_SQLiteDB);
	if (!hob) RETURN_ERROR("[SQLITE] Failed to allocate a handle!");
	ctx = (SQLITE_CONTEXT*)hob->data;

	// Initialize embedded sqlite-vec extension.
	// (Maybe it should be done on the request only...)
	rc = sqlite3_auto_extension((void(*)(void))sqlite3_vec_init);
	if(rc != SQLITE_OK) goto error;

	rc = sqlite3_open(SERIES_TEXT(filename), &ctx->db);
	if(rc != SQLITE_OK) goto error;

	RETURN_HANDLE(hob);

error:
	snprintf(error_buffer, 254, "[SQLITE] %s", sqlite3_errstr(rc));
	RETURN_ERROR(error_buffer);
}

COMMAND cmd_sqlite_close(RXIFRM* frm, void* reb_ctx) {
	REBHOB  *hob;
	SQLITE_CONTEXT *ctx;

	RESOLVE_SQLITE_CTX(ctx, 1);
	if(ctx && ctx->db) {
		sqlite3_close(ctx->db);
		ctx->db = NULL;
	}
	return RXR_UNSET;
}

COMMAND cmd_sqlite_last_insert_id(RXIFRM* frm, void* reb_ctx) {
	REBHOB  *hob;
	SQLITE_CONTEXT *ctx;

	RESOLVE_SQLITE_CTX(ctx, 1);

	RXA_INT64(frm, 1) = sqlite3_last_insert_rowid(ctx->db);
	RXA_TYPE (frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

static int trace_callback(unsigned type, void* ctx, void* pStmt, void* pValue) {
	uint64_t* duration;

	printf("TRACE[%u] ", type);

	switch(type) {
		case SQLITE_TRACE_STMT:
			printf("STMT: %s\n", sqlite3_expanded_sql((sqlite3_stmt*)pStmt));
			break;
		case SQLITE_TRACE_PROFILE:
			duration = pValue;
			printf("PROFILE: %lluns\n", *duration);
			break;
		case SQLITE_TRACE_ROW:
			puts("ROW");
			break;
		case SQLITE_TRACE_CLOSE:
			puts("CLOSE");
			break;
		default:
			puts("unknown");
			break;
	}
	fflush(stdout);
	return SQLITE_OK;
}

COMMAND cmd_sqlite_trace(RXIFRM* frm, void* reb_ctx) {
	REBHOB  *hob;
	SQLITE_CONTEXT *ctx;
	unsigned mask;

	RESOLVE_SQLITE_CTX(ctx, 1);
	if(ctx && ctx->db) {
		mask = RXA_INT64(frm, 2) & 0x0F;
		//printf("mask: %u\n", mask);
		sqlite3_trace_v2(ctx->db, mask, trace_callback, ctx);
	}
	return RXR_UNSET;
}


//== exec / eval ===============================================================

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
	int i;
	for(i=0; i<argc; i++){
		RL_PRINT(b_cast("%s = %s\n"), azColName[i], argv[i] ? argv[i] : "NULL");
	}
	RL_PRINT(b_cast("\n"),0);
	return 0;
}

COMMAND cmd_sqlite_exec(RXIFRM* frm, void* reb_ctx) {
	REBHOB  *hob;
	REBSER  *sql;
	SQLITE_CONTEXT *ctx;
	sqlite3 *db = NULL;
	int rc;

	RESOLVE_SQLITE_CTX(ctx, 1);
	sql = utf8_string(RXA_ARG(frm, 2));

	db = ctx->db;

	//debug_print("exec  DB: %p\n", (void*)db);
	//debug_print("exec SQL: %s\n", SERIES_TEXT(sql));
	rc = sqlite3_exec(db, SERIES_TEXT(sql), callback, 0, 0);

	//debug_print("exec result: %i\n", rc);
	if( rc!=SQLITE_OK ){
		snprintf(error_buffer, 254, "[SQLITE] %s %s", sqlite3_errstr(rc), sqlite3_errmsg(db));
		RETURN_ERROR(error_buffer);
	}

	return RXR_UNSET;
}

COMMAND cmd_sqlite_eval(RXIFRM* frm, void* reb_ctx) {
	REBHOB  *hob;
	REBHOB  *hobStmt;
	REBSER  *sql;
	REBCNT   index = 0;
	REBCNT   row, maxRows;
	REBINT   type, col, columns, bytes;

	REBSER  *ser;
	REBSER  *params = NULL;
	REBSER  *result = NULL;
	REBYTE  *bin;
	RXIARG   arg = {0};
	REBOOL   freeStmt = FALSE;

	SQLITE_STMT    *ctxStmt = NULL;
	SQLITE_CONTEXT *ctx;
	sqlite3        *db   = NULL;
	sqlite3_stmt   *stmt = NULL;
	int rc = SQLITE_OK;
	int ret = RXR_UNSET;

	maxRows = (REBCNT)-1; //TODO: it should be user defined

	RESOLVE_SQLITE_CTX(ctx, 1);
	db = ctx->db;

	ctx->last_insert_count = 0;

	if (RXA_TYPE(frm,2) == RXT_HANDLE) {
		RESOLVE_SQLITE_STMT(ctxStmt, 2);
		stmt = ctxStmt->stmt;
	}
	else if (RXA_TYPE(frm,2) == RXT_STRING) {
		// evaluate single or more semicolon separated statemens using the sqlite_exec function
		sql = utf8_string(RXA_ARG(frm, 2));
		rc = sqlite3_prepare_v2(db, SERIES_TEXT(sql), SERIES_TAIL(sql), &stmt, 0);
		if( rc!=SQLITE_OK ) goto error;
		freeStmt = TRUE;
		//debug_print("SQL: %s\n", SERIES_TEXT(sql));
	}
	else if (RXA_TYPE(frm,2) == RXT_BLOCK) {
		params = RXA_SERIES(frm, 2);
		index  = RXA_INDEX(frm, 2);
		type = RL_GET_VALUE_RESOLVED(params, index, &arg);
		if (type == RXT_STRING) {
			sql = utf8_string(arg);
			rc = sqlite3_prepare_v2(db, SERIES_TEXT(sql), SERIES_TAIL(sql), &stmt, 0);
			if( rc!=SQLITE_OK ) goto error;
			freeStmt = TRUE;
		}
		else if (type == RXT_HANDLE) {
			hobStmt = arg.handle.hob;
			if (hobStmt->sym != Handle_SQLiteSTMT) {
				rc = SQLITE_MISUSE;
				goto error;
			}
			ctxStmt = (SQLITE_STMT*)hobStmt->data;
			stmt = ctxStmt->stmt;
		}
		else {
			rc = SQLITE_MISUSE;
			goto error;
		}
		index++;
	}

	// bind statement's parameters...
	if (params) {
		//if (ctxStmt) debug_print("ctxStmt->last_result_code = %i\n", ctxStmt->last_result_code);
		if (ctxStmt && ctxStmt->last_result_code == SQLITE_DONE) sqlite3_reset(stmt);
		rc = bind_parameters(stmt, params, &index);
		if (rc != SQLITE_OK) goto finish;
	}

	// evaluate single statement using the sqlite_step function
	for (row = 0; row < maxRows; row++) {
		rc = sqlite3_step(stmt);
		if (ctxStmt) ctxStmt->last_result_code = rc;
		//debug_print("row: %i = step result: %i, requested rows: %u\n", row, rc, maxRows);

		switch(rc) {
			case SQLITE_ROW:
				if (!result) {
					columns = sqlite3_data_count(stmt);
					//debug_print("step has data: %i columns\n", columns);

					// preallocate the block to hold results...
					result = RL_MAKE_BLOCK(columns);
					RXA_SERIES(frm, 1) = result;
					RXA_TYPE  (frm, 1) = RXT_BLOCK;
					RXA_INDEX (frm, 1) = 0;
				}

				//debug_print("SERIES_TAIL(s) = %u SERIES_REST(s) = %u\n", SERIES_TAIL(result), SERIES_REST(result));

				CLEARS(&arg);
				for(col = 0; col < columns; col++) {
					type = sqlite3_column_type(stmt, col);
					//debug_print("column[%ix%i] type: %i\n", row, col, type);
					switch(type) {
						case SQLITE_INTEGER:
							type = RXT_INTEGER;
							arg.int64 = sqlite3_column_int64(stmt, col);
							break;
						case SQLITE_FLOAT:
							type = RXT_DECIMAL;
							arg.dec64 = sqlite3_column_double(stmt, col);
							break;
						case SQLITE_TEXT:
							type = RXT_STRING;
							bytes = sqlite3_column_bytes(stmt, col);
							arg.series = RL_DECODE_UTF_STRING((REBYTE*)sqlite3_column_text(stmt, col), bytes, 8, 0, 0);
							break;
						case SQLITE_BLOB:
							type = RXT_BINARY;
							bytes = sqlite3_column_bytes(stmt, col);
							bin = (REBYTE*)sqlite3_column_blob(stmt, col);
							if (bin) {
								ser = RL_MAKE_BINARY(bytes);
								memcpy(SERIES_DATA(ser), bin, bytes);
								SERIES_TAIL(ser) = bytes;
								arg.series = ser;
							}
							break;
						case SQLITE_NULL:
							type = RXT_NONE;
					}
					// Append the new column value into the result.
					// It also expands the series if there is no room and updates its tail.
					RL_SET_VALUE(result, (row * columns) + col, arg, type);
				}
				break;
			case SQLITE_DONE:
				//trace("step done");
				if(result) return RXR_VALUE;
				ctx->last_insert_count += sqlite3_changes(db);
				sqlite3_reset(stmt);

				if (params && index < SERIES_TAIL(params)) {
					//debug_print("bind_parameters index: %i\n", index);
					rc = bind_parameters(stmt, params, &index);
					if (rc != SQLITE_OK) goto finish;
					continue;
				}
				if (freeStmt) sqlite3_finalize(stmt);
				RXA_INT64(frm, 1) = ctx->last_insert_count;
				RXA_TYPE (frm, 1) = RXT_INTEGER;

				return RXR_VALUE;

			default:
				//rc = sqlite3_reset(stmt);
				goto finish;
		}
	}


finish:
	if (freeStmt) sqlite3_finalize(stmt);
	if( rc!=SQLITE_OK ){
error:
		snprintf(error_buffer, 254, "[SQLITE] %s", sqlite3_errstr(rc));
		RETURN_ERROR(error_buffer);
	}

	return ret;
}


//== prepared statements =======================================================

COMMAND cmd_sqlite_prepare(RXIFRM* frm, void* reb_ctx) {
	REBHOB  *hob;
	REBHOB  *hobStmt;
	REBSER  *sql;
	SQLITE_CONTEXT *ctx;
	SQLITE_STMT *ctxStmt;
	sqlite3 *db = NULL;
	int rc;

	RESOLVE_SQLITE_CTX(ctx, 1);
	sql = utf8_string(RXA_ARG(frm, 2));

	db = ctx->db;

	//debug_print("prep  DB: %p\n", (void*)db);
	//debug_print("prep SQL: %s\n", SERIES_TEXT(sql));

	hobStmt = RL_MAKE_HANDLE_CONTEXT(Handle_SQLiteSTMT);
	ctxStmt = (SQLITE_STMT*)hobStmt->data;

	rc = sqlite3_prepare_v2(db, SERIES_TEXT(sql), SERIES_TAIL(sql), &ctxStmt->stmt, 0);

	//debug_print("prep result: %i\n", rc);
	if( rc!=SQLITE_OK ){
		snprintf(error_buffer, 254, "[SQLITE] %s %s", sqlite3_errstr(rc), sqlite3_errmsg(db));
		RETURN_ERROR(error_buffer);
	}

	ctxStmt->last_result_code = SQLITE_ROW;

	RETURN_HANDLE(hobStmt);
}

COMMAND cmd_sqlite_reset(RXIFRM* frm, void* reb_ctx) {
	REBHOB      *hobStmt;
	SQLITE_STMT *ctxStmt;

	RESOLVE_SQLITE_STMT(ctxStmt, 1);
	sqlite3_reset(ctxStmt->stmt);
	ctxStmt->last_result_code = SQLITE_ROW;

	return RXR_UNSET;
}

COMMAND cmd_sqlite_finalize(RXIFRM* frm, void* reb_ctx) {
	REBHOB      *hobStmt;
	SQLITE_STMT *ctxStmt;

	RESOLVE_SQLITE_STMT(ctxStmt, 1);
	sqlite3_finalize(ctxStmt->stmt);
	ctxStmt->stmt = NULL;
	return RXR_UNSET;
}

COMMAND cmd_sqlite_columns(RXIFRM* frm, void* reb_ctx) {
	REBHOB  *hobStmt;
	REBSER  *blk = NULL;
	REBINT   count, i;
	RXIARG   arg = {0};
	SQLITE_STMT *ctxStmt;
	const char *name;

	RESOLVE_SQLITE_STMT(ctxStmt, 1);

	count = sqlite3_column_count(ctxStmt->stmt);

	debug_print("column_count: %i\n", count);
	if (count == 0) return RXR_NONE;
	blk = RL_MAKE_BLOCK(count);
	for (i = 0; i < count; i++) {
		name = sqlite3_column_name(ctxStmt->stmt, i);
		// It is not possible to return names as words, because RL_MAP_WORD
		// is able to create invalid words (containing invalid characters)
		// That is possible with queries like: SELECT randomblob(16);

		// So just return names as strings...
		arg.series = RL_DECODE_UTF_STRING((REBYTE*)name, strlen(name), 8, 0, 0);
		RL_SET_VALUE(blk, SERIES_TAIL(blk), arg, RXT_STRING);
	}

	RXA_SERIES(frm, 1) = blk;
	RXA_TYPE  (frm, 1) = RXT_BLOCK;
	RXA_INDEX (frm, 1) = 0;
	return RXR_VALUE;
}

COMMAND cmd_sqlite_step(RXIFRM* frm, void* reb_ctx) {
	REBHOB  *hobStmt;
	REBSER  *ser;
	REBSER  *str;
	REBSER  *blk = NULL;
	REBYTE  *bin;
	RXIARG   arg = {0};
	SQLITE_STMT *ctxStmt;
	sqlite3_stmt *stmt;
	int rc, columns = 0, bytes, row, col, type;
	int refRows, allRows = 0;
	i64 maxRows, rows;

	RESOLVE_SQLITE_STMT(ctxStmt, 1);
	refRows = RXA_REF(frm, 2);
	maxRows = RXA_INT64(frm, 3);
	if (!refRows) maxRows = 1;
	if (maxRows < 1) allRows = maxRows = 1;
	rows = (maxRows > 1000) ? 1000 : maxRows; //no need to use exact pow2 numbers, Rebol will do it anyway

	stmt = ctxStmt->stmt;

	if (RXA_REF(frm, 4)) { // with
		ser = RXA_SERIES(frm, 5);

		//sqlite3_reset() does not reset the bindings on a prepared statement!
		sqlite3_clear_bindings(stmt);

		for(col = 0; col < (REBINT)SERIES_TAIL(ser); col++) {
			type = RL_GET_VALUE(ser, col, &arg);
			//printf("arg type: %i\n", type);
			if (type == RXT_WORD || type == RXT_GET_WORD || type == RXT_GET_PATH) {
				type = RL_GET_VALUE_RESOLVED(ser, col, &arg);
			}
			//printf("arg type: %i\n", type);
			rc = -1;
			switch(type) {
				case RXT_INTEGER:
					rc = sqlite3_bind_int64(stmt, col+1, arg.int64);
					break;
				case RXT_DECIMAL:
					rc = sqlite3_bind_double(stmt, col+1, arg.dec64);
					break;
				case RXT_STRING:
				case RXT_FILE:
				case RXT_EMAIL:
				case RXT_REF:
				case RXT_URL:
				case RXT_TAG:
					// Make sure to convert unicode string to UTF-8
					str = (REBSER*)arg.series;
					if (SERIES_WIDE(str) > 1) {
						str = RL_ENCODE_UTF8_STRING(SERIES_DATA(str), SERIES_TAIL(str), TRUE, FALSE);
						arg.index = 0;
					}
					rc = sqlite3_bind_text(stmt, col+1, SERIES_SKIP(str, arg.index), SERIES_TAIL(str)-arg.index, SQLITE_TRANSIENT);
					break;
				case RXT_NONE:
					rc = sqlite3_bind_null(stmt, col+1);
					break;
				case RXT_LOGIC:
					rc = sqlite3_bind_int(stmt, col+1, arg.int32a);
					break;
				case RXT_BINARY:
					str = (REBSER*)arg.series;
					rc = sqlite3_bind_blob(stmt, col+1, SERIES_SKIP(str, arg.index), SERIES_TAIL(str)-arg.index, SQLITE_TRANSIENT);
					break;
				case RXT_VECTOR:
					str = arg.vector.series;
					rc = sqlite3_bind_blob(stmt, col+1, SERIES_DATA(str), SERIES_TAIL(str) * (RXI_VECTOR_BITS(arg.vector.info) / 8), SQLITE_TRANSIENT);
					break;
			}
			if (rc < 0) RETURN_ERROR("[SQLITE] Unsupported value type!");
			//debug_print("bind result: %i\n", rc);
		}
	}

	rc = sqlite3_stmt_readonly(stmt) ? ctxStmt->last_result_code : SQLITE_ROW;

	for (row = 0; rc == SQLITE_ROW && (allRows || row < maxRows); row++) {
		rc = sqlite3_step(stmt);
		ctxStmt->last_result_code = rc;
		//debug_print("row: %i = step result: %i, requested rows: %li allRows: %i\n", row, rc, maxRows, allRows);
		switch(rc) {
			case SQLITE_ROW:
				if (!blk) {
					columns = sqlite3_data_count(stmt);
					//debug_print("step has data: %i columns\n", columns);

					// preallocate the block to hold results...
					blk = RL_MAKE_BLOCK(columns * (REBCNT)rows);
					RXA_SERIES(frm, 1) = blk;
					RXA_TYPE  (frm, 1) = RXT_BLOCK;
					RXA_INDEX (frm, 1) = 0;
				}

				//debug_print("SERIES_TAIL(s) = %u SERIES_REST(s) = %u\n", SERIES_TAIL(blk), SERIES_REST(blk));

				CLEARS(&arg);
				for(col = 0; col < columns; col++) {
					type = sqlite3_column_type(stmt, col);
					//debug_print("column[%ix%i] type: %i\n", row, col, type);
					switch(type) {
						case SQLITE_INTEGER:
							type = RXT_INTEGER;
							arg.int64 = sqlite3_column_int64(stmt, col);
							break;
						case SQLITE_FLOAT:
							type = RXT_DECIMAL;
							arg.dec64 = sqlite3_column_double(stmt, col);
							break;
						case SQLITE_TEXT:
							type = RXT_STRING;
							bytes = sqlite3_column_bytes(stmt, col);
							arg.series = RL_DECODE_UTF_STRING((REBYTE*)sqlite3_column_text(stmt, col), bytes, 8, 0, 0);
							break;
						case SQLITE_BLOB:
							type = RXT_BINARY;
							bytes = sqlite3_column_bytes(stmt, col);
							bin = (REBYTE*)sqlite3_column_blob(stmt, col);
							if (bin) {
								ser = RL_MAKE_BINARY(bytes);
								memcpy(SERIES_DATA(ser), bin, bytes);
								SERIES_TAIL(ser) = bytes;
								arg.series = ser;
							}
							break;
						case SQLITE_NULL:
							type = RXT_NONE;
					}
					// Append the new column value into the result.
					// It also expands the series if there is no room and updates its tail.
					RL_SET_VALUE(blk, (row * columns) + col, arg, type);
				}
				break;
			case SQLITE_DONE:
				//trace("step done");
				if(blk) return RXR_VALUE;
				sqlite3_reset(stmt);
				return RXR_NONE;
			case SQLITE_BUSY:
				//trace("step busy");
				RETURN_ERROR("[SQLITE] Statement is busy!");
			case SQLITE_ERROR:
				rc = sqlite3_reset(stmt);
				RETURN_ERROR(sqlite3_errstr(rc));
			case SQLITE_MISUSE:
				//trace("step misuse");
				RETURN_ERROR("[SQLITE] Statement misuse!");
		}
	}
	//debug_print("step result: %i\n", rc);
	if (rc < SQLITE_ROW && rc != SQLITE_OK) {
		rc = sqlite3_reset(stmt);
		RETURN_ERROR(sqlite3_errstr(rc));
	}
	if(blk) return RXR_VALUE;
	return RXR_NONE;
}


//== handle release callbacks ==================================================

int SQLiteDBHandle_release(void *ctx) {
	SQLITE_CONTEXT *c = (SQLITE_CONTEXT*)ctx;
	debug_print("releasing sqlite db: %p\n", c->db);
	if (c->db) sqlite3_close((sqlite3*)c->db);
	return 0;
}

int SQLiteSTMTHandle_release(void *ctx) {
	SQLITE_STMT *c = (SQLITE_STMT*)ctx;
	debug_print("releasing sqlite stmt: %p\n", c->stmt);
	if (c->stmt) sqlite3_finalize((sqlite3_stmt*)c->stmt);
	return 0;
}
