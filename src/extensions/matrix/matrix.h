//
// Project: Rebol/Matrix extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Shared between the entry points (matrix.c) and the command sources.
//
// Shape convention:
//   rows  = RXI_VECTOR_ROWS(info), floored at 1 - a plain vector reports
//           Nx1, matching what `query`/`reflect` report on the Rebol side,
//           so there is no none-versus-pair case to handle here
//   cols  = len / rows
//   element (i,j) lives at linear index i*cols + j (row-major; 0-based here,
//           while Rebol's pick/poke are 1-based)
//   a partial view (index > 0) has no shape of its own: it is treated as a
//           single row of the elements it can see, mirroring Vector_Rows_For
//           on the core side. An empty view is likewise one empty row.
//
// DETERMINANT, INVERT and SOLVE work in double regardless of the input
// element type and always produce float64! results; every other command
// keeps the element type of its input.
//

#ifndef MATRIX_EXT_H
#define MATRIX_EXT_H

#define ERR_NO_VECTOR     ((const REBYTE*)"Invalid vector argument!")
#define ERR_NOT_SQUARE    ((const REBYTE*)"Matrix must be square!")
#define ERR_NO_RESULT     ((const REBYTE*)"Cannot allocate the result matrix!")
#define ERR_ROW_RANGE     ((const REBYTE*)"Row index out of range!")
#define ERR_BAD_DIMS      ((const REBYTE*)"Columns of the first matrix must match rows of the second!")
#define ERR_BAD_RHS       ((const REBYTE*)"Right-hand side must be a column matrix with one row per equation!")
#define ERR_TYPE_MISMATCH ((const REBYTE*)"Both matrices must have the same element type!")
#define ERR_PARTIAL_VIEW  ((const REBYTE*)"Matrix must be at its head!")
#define ERR_SINGULAR      ((const REBYTE*)"Matrix is singular!")

// A vector argument resolved into its parts.
typedef struct {
	REBSER *ser;
	REBYTE *data;    // points at the view's first element, not the buffer head
	REBCNT  info;    // packed element type + rows
	REBLEN  tail;    // whole series
	REBLEN  index;   // where this value's view starts
	REBLEN  len;     // visible element count (tail - index)
	REBLEN  rows;    // 1 for a plain vector, a partial view, or an empty one
	REBLEN  cols;    // len / rows
	REBCNT  wide;    // element width in bytes
} Matrix;

// The helpers below are prefixed rather than static: embedded extensions all
// link into one binary, so an unadorned `matrix_arg` would collide with the
// next extension that happens to pick the same helper name.

// Fills `m` from command argument `n`.
// Returns 0 only when the argument carries no series. An empty vector
// SUCCEEDS, with len == 0, cols == 0 and rows == 1, so each command decides
// for itself whether empty is an error (the square-only ones fail their
// IS_SQUARE test) or passes through as an empty result.
int Matrix_Arg(Matrix *m, RXIFRM *frm, int n);

// Allocates a result vector with the given shape and stores it into argument
// 1 of the frame. Returns the result's data pointer, or NULL on failure.
//
// Matrix_Result takes the element type from `src`; Matrix_Result_Type takes
// it explicitly, for commands whose result type differs from the input.
//
// Storing into the frame drops the source's last reference, so call this
// AFTER the last allocation and BEFORE reading the source data: no
// allocation may happen between the call and the final read.
REBYTE *Matrix_Result(RXIFRM *frm, Matrix *src, REBLEN cols, REBLEN rows);
REBYTE *Matrix_Result_Type(RXIFRM *frm, REBCNT vtype, REBLEN cols, REBLEN rows);

#define IS_SQUARE(m) ((m).rows == (m).cols && (m).rows > 0)

// A command that mutates its argument in place (identity) must refuse a
// partial view: rows collapses to 1 there, so a one-element view would pass
// as a 1x1 matrix and overwrite the wrong element.
#define IS_WHOLE(m)  ((m).index == 0 && (m).len == (m).tail)

// Dispatches a body macro over the concrete C element type.
//
// Needed wherever a command has to interpret the bytes - arithmetic, or
// simply reading elements out into a double workspace. Commands that merely
// relocate elements (transpose, rotate, diagonal, swap-rows) copy by element
// width instead, which is both type-agnostic and lossless.
#define VEC_DISPATCH(info, BODY) do {                                     \
	REBCNT _bits = RXI_VECTOR_BITS(info);                                 \
	if (RXI_VECTOR_FLOAT(info)) {                                         \
		if (_bits == 32) { BODY(float);  } else { BODY(double); }         \
	} else if (RXI_VECTOR_SIGNED(info)) {                                 \
		switch (_bits) {                                                  \
		case  8: BODY(i8);  break;                                        \
		case 16: BODY(i16); break;                                        \
		case 32: BODY(i32); break;                                        \
		default: BODY(i64); break;                                        \
		}                                                                 \
	} else {                                                              \
		switch (_bits) {                                                  \
		case  8: BODY(u8);  break;                                        \
		case 16: BODY(u16); break;                                        \
		case 32: BODY(u32); break;                                        \
		default: BODY(u64); break;                                        \
		}                                                                 \
	}                                                                     \
} while(0)

// As VEC_DISPATCH, but the body also receives a wide accumulator type:
// i64/u64 for integers, double for both float widths.
//
// A product of two elements can overflow the element type - which is
// undefined behaviour for the signed types, and loses precision for f32 -
// so arithmetic accumulates wide and truncates only on store. That matches
// how the core's own vector math behaves.
#define VEC_DISPATCH_ACC(info, BODY) do {                                    \
	REBCNT _bits = RXI_VECTOR_BITS(info);                                    \
	if (RXI_VECTOR_FLOAT(info)) {                                            \
		if (_bits == 32) { BODY(float, double); } else { BODY(double, double); } \
	} else if (RXI_VECTOR_SIGNED(info)) {                                    \
		switch (_bits) {                                                     \
		case  8: BODY(i8,  i64); break;                                      \
		case 16: BODY(i16, i64); break;                                      \
		case 32: BODY(i32, i64); break;                                      \
		default: BODY(i64, i64); break;                                      \
		}                                                                    \
	} else {                                                                 \
		switch (_bits) {                                                     \
		case  8: BODY(u8,  u64); break;                                      \
		case 16: BODY(u16, u64); break;                                      \
		case 32: BODY(u32, u64); break;                                      \
		default: BODY(u64, u64); break;                                      \
		}                                                                    \
	}                                                                        \
} while(0)


#endif // MATRIX_EXT_H