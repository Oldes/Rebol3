//
// Project: Rebol/Matrix extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Command implementations.
//
// See matrix.h for the shape convention and the shared helpers.
//

#include "gen-matrix.h"
#include "matrix.h"
#include <string.h>
#include <stdlib.h>

//== helpers ==================================================================

int Matrix_Arg(Matrix *m, RXIFRM *frm, int n) {
	m->ser  = RXA_VECTOR_SERIES(frm, n);
	m->info = RXA_VECTOR_INFO(frm, n);
	if (!m->ser) return 0;

	m->tail  = (REBLEN)RL_SERIES(m->ser, RXI_SER_TAIL);
	m->index = RXA_VECTOR_INDEX(frm, n);      // confirm the macro name
	if (m->index > m->tail) m->index = m->tail;
	m->len   = m->tail - m->index;
	m->wide  = RXI_VECTOR_BITS(m->info) / 8;
	m->data  = (REBYTE*)RL_SERIES(m->ser, RXI_SER_DATA) + m->index * m->wide;

	// Mirrors Vector_Rows_For on the core side: a partial or empty view is
	// one row of what it can see.
	m->rows = (m->len && IS_WHOLE(*m)) ? RXI_VECTOR_ROWS(m->info) : 1;
	if (m->rows < 1) m->rows = 1;
	m->cols = m->len / m->rows;
	return 1;                                  // empty is no longer a failure
}

static REBYTE *Matrix_Empty(RXIFRM *frm, Matrix *m) {
	return Matrix_Result(frm, m, 0, 1);
}

REBYTE *Matrix_Result_Type(RXIFRM *frm, REBCNT vtype, REBLEN cols, REBLEN rows) {
	RXIARG out;
	if (!RL_MAKE_VECTOR(&out, vtype, (REBINT)cols, (REBINT)rows))
		return NULL;
	RXA_ARG(frm, 1)  = out;
	RXA_TYPE(frm, 1) = RXT_VECTOR;
	return (REBYTE*)RL_SERIES(out.vector.series, RXI_SER_DATA);
}
REBYTE *Matrix_Result(RXIFRM *frm, Matrix *src, REBLEN cols, REBLEN rows) {
	return Matrix_Result_Type(frm, RXI_VECTOR_TYPE(src->info), cols, rows);
}


//== element movement =========================================================
// These never interpret the bytes, so one width-based path covers every
// element type without loss.

COMMAND cmd_matrix_transpose(RXIFRM *frm, void *ctx) {
	Matrix m;
	REBYTE *dst;
	REBLEN i, j, w;

	if (!Matrix_Arg(&m, frm, 1)) RETURN_ERROR(ERR_NO_SHAPE);
	if (!m.len) { if (!Matrix_Empty(frm, &m)) RETURN_ERROR(ERR_NO_RESULT); return RXR_VALUE; }

	// result is the same data with the dimensions exchanged
	if (!(dst = Matrix_Result(frm, &m, m.rows, m.cols)))
		RETURN_ERROR(ERR_NO_RESULT);

	w = m.wide;
	for (i = 0; i < m.rows; i++) {
		for (j = 0; j < m.cols; j++) {
			memcpy(dst + (j * m.rows + i) * w,
			       m.data + (i * m.cols + j) * w, w);
		}
	}
	return RXR_VALUE;
}

COMMAND cmd_matrix_diagonal(RXIFRM *frm, void *ctx) {
	Matrix m;
	REBYTE *dst;
	REBLEN i, n, w;

	if (!Matrix_Arg(&m, frm, 1)) RETURN_ERROR(ERR_NO_SHAPE);
	if (!m.len) { if (!Matrix_Empty(frm, &m)) RETURN_ERROR(ERR_NO_RESULT); return RXR_VALUE; }

	n = m.rows < m.cols ? m.rows : m.cols;
	// returned as a plain (unshaped) vector: rows = 1
	if (!(dst = Matrix_Result(frm, &m, n, 1)))
		RETURN_ERROR(ERR_NO_RESULT);

	w = m.wide;
	for (i = 0; i < n; i++)
		memcpy(dst + i * w, m.data + (i * m.cols + i) * w, w);

	return RXR_VALUE;
}

COMMAND cmd_matrix_swap_rows(RXIFRM *frm, void *ctx) {
	Matrix m;
	REBLEN a, b, row_bytes, k;
	REBYTE *pa, *pb, tmp;

	if (!Matrix_Arg(&m, frm, 1)) RETURN_ERROR(ERR_NO_SHAPE);

	a = (REBLEN)ARG_Int64(2);
	b = (REBLEN)ARG_Int64(3);
	if (a < 1 || b < 1 || a > m.rows || b > m.rows)
		RETURN_ERROR(ERR_ROW_RANGE);

	if (a != b) {
		row_bytes = m.cols * m.wide;
		pa = m.data + (a - 1) * row_bytes;
		pb = m.data + (b - 1) * row_bytes;
		// in place, no scratch buffer
		for (k = 0; k < row_bytes; k++) {
			tmp = pa[k]; pa[k] = pb[k]; pb[k] = tmp;
		}
	}
	// modifies in place and returns the same value
	return RXR_VALUE;
}

COMMAND cmd_matrix_rotate(RXIFRM *frm, void *ctx) {
	Matrix m;
	REBYTE *dst;
	REBLEN i, j, w, di, dj, drows, dcols;
	REBFLG ref_left  = RXA_REF(frm, 2);
	REBFLG ref_twice = RXA_REF(frm, 3);

	if (!Matrix_Arg(&m, frm, 1)) RETURN_ERROR(ERR_NO_SHAPE);
	if (!m.len) { if (!Matrix_Empty(frm, &m)) RETURN_ERROR(ERR_NO_RESULT); return RXR_VALUE; }

	if (ref_twice) {                // 180 degrees - shape is unchanged
		drows = m.rows; dcols = m.cols;
	} else {                        // 90 degrees - dimensions exchange
		drows = m.cols; dcols = m.rows;
	}
	if (!(dst = Matrix_Result(frm, &m, dcols, drows)))
		RETURN_ERROR(ERR_NO_RESULT);

	w = m.wide;
	for (i = 0; i < m.rows; i++) {
		for (j = 0; j < m.cols; j++) {
			if (ref_twice) {                // 180
				di = m.rows - 1 - i;
				dj = m.cols - 1 - j;
			} else if (ref_left) {          // 90 counter-clockwise
				di = m.cols - 1 - j;
				dj = i;
			} else {                        // 90 clockwise
				di = j;
				dj = m.rows - 1 - i;
			}
			memcpy(dst + (di * dcols + dj) * w,
			       m.data + (i * m.cols + j) * w, w);
		}
	}
	return RXR_VALUE;
}


//== value-producing operations ===============================================

COMMAND cmd_matrix_identity(RXIFRM *frm, void *ctx) {
	Matrix m;
	REBLEN i;

	if (!Matrix_Arg(&m, frm, 1)) RETURN_ERROR(ERR_NO_SHAPE);
	if (!IS_WHOLE(m)) RETURN_ERROR(ERR_PARTIAL_VIEW);
	if (!IS_SQUARE(m)) RETURN_ERROR(ERR_NOT_SQUARE);

	memset(m.data, 0, m.tail * m.wide);

	#define IDENTITY_BODY(T) do {                              \
		T *p = (T*)m.data;                                     \
		for (i = 0; i < m.rows; i++) p[i * m.cols + i] = (T)1; \
	} while(0)
	VEC_DISPATCH(m.info, IDENTITY_BODY);
	#undef IDENTITY_BODY

	// modifies in place and returns the same value
	return RXR_VALUE;
}

COMMAND cmd_matrix_trace(RXIFRM *frm, void *ctx) {
	Matrix m;
	REBLEN i;
	double fsum = 0.0;
	i64    isum = 0;

	if (!Matrix_Arg(&m, frm, 1)) RETURN_ERROR(ERR_NO_SHAPE);
	if (!IS_SQUARE(m))           RETURN_ERROR(ERR_NOT_SQUARE);

	if (RXI_VECTOR_FLOAT(m.info)) {
		#define TRACE_F(T) do {                                            \
			T *p = (T*)m.data;                                             \
			for (i = 0; i < m.rows; i++) fsum += (double)p[i * m.cols + i];\
		} while(0)
		VEC_DISPATCH(m.info, TRACE_F);
		#undef TRACE_F
		RXA_DEC64(frm, 1) = fsum;
		RXA_TYPE(frm, 1)  = RXT_DECIMAL;
	} else {
		#define TRACE_I(T) do {                                            \
			T *p = (T*)m.data;                                             \
			for (i = 0; i < m.rows; i++) isum += (i64)p[i * m.cols + i];   \
		} while(0)
		VEC_DISPATCH(m.info, TRACE_I);
		#undef TRACE_I
		RXA_INT64(frm, 1) = isum;
		RXA_TYPE(frm, 1)  = RXT_INTEGER;
	}
	return RXR_VALUE;
}

COMMAND cmd_matrix_matmul(RXIFRM *frm, void *ctx) {
	Matrix a, b;
	REBYTE *dst;
	REBLEN i, j, k;

	if (!Matrix_Arg(&a, frm, 1) || !Matrix_Arg(&b, frm, 2))
		RETURN_ERROR(ERR_NO_SHAPE);
	if (a.cols != b.rows)
		RETURN_ERROR(ERR_BAD_DIMS);
	if (RXI_VECTOR_TYPE(a.info) != RXI_VECTOR_TYPE(b.info))
		RETURN_ERROR(ERR_TYPE_MISMATCH);

	if (!(dst = Matrix_Result(frm, &a, b.cols, a.rows)))
		RETURN_ERROR(ERR_NO_RESULT);

	#define MATMUL_BODY(T, ACC) do {                                \
		T *pa = (T*)a.data, *pb = (T*)b.data, *pd = (T*)dst;        \
		for (i = 0; i < a.rows; i++)                                \
			for (j = 0; j < b.cols; j++) {                          \
				ACC sum = (ACC)0;                                   \
				for (k = 0; k < a.cols; k++)                        \
					sum += (ACC)pa[i * a.cols + k]                  \
					     * (ACC)pb[k * b.cols + j];                 \
				pd[i * b.cols + j] = (T)sum;                        \
			}                                                       \
	} while(0)
	VEC_DISPATCH_ACC(a.info, MATMUL_BODY);
	#undef MATMUL_BODY

	return RXR_VALUE;
}

COMMAND cmd_matrix_kronecker(RXIFRM *frm, void *ctx) {
	Matrix a, b;
	REBYTE *dst;
	REBLEN i, j, k, l, dcols;

	if (!Matrix_Arg(&a, frm, 1) || !Matrix_Arg(&b, frm, 2))
		RETURN_ERROR(ERR_NO_SHAPE);
	if (RXI_VECTOR_TYPE(a.info) != RXI_VECTOR_TYPE(b.info))
		RETURN_ERROR(ERR_TYPE_MISMATCH);

	dcols = a.cols * b.cols;
	if (!(dst = Matrix_Result(frm, &a, dcols, a.rows * b.rows)))
		RETURN_ERROR(ERR_NO_RESULT);

	#define KRON_BODY(T, ACC) do {                                   \
		T *pa = (T*)a.data, *pb = (T*)b.data, *pd = (T*)dst;         \
		for (i = 0; i < a.rows; i++)                                 \
		  for (j = 0; j < a.cols; j++) {                             \
		    ACC av = (ACC)pa[i * a.cols + j];                        \
		    for (k = 0; k < b.rows; k++)                             \
		      for (l = 0; l < b.cols; l++)                           \
		        pd[(i * b.rows + k) * dcols + (j * b.cols + l)]      \
		            = (T)(av * (ACC)pb[k * b.cols + l]);             \
		  }                                                          \
	} while(0)
	VEC_DISPATCH_ACC(a.info, KRON_BODY);
	#undef KRON_BODY

	return RXR_VALUE;
}

#define DABS(x) ((x) < 0.0 ? -(x) : (x))
 
 
//== LU decomposition =========================================================
 
typedef struct {
	double *a;      // n*n row-major, overwritten with the LU factors
	REBLEN *perm;   // row i of the factored matrix came from perm[i]
	REBLEN  n;
	int     sign;   // +1/-1 from the row swaps; 0 means singular
} LU;
 
static void LU_Free(LU *lu) {
	if (lu->a)    free(lu->a);
	if (lu->perm) free(lu->perm);
	lu->a = NULL;
	lu->perm = NULL;
}
 
// Copies `m` into a double workspace and factors it in place.
// Returns 0 only on allocation failure; a singular matrix succeeds with
// lu->sign set to 0, so the caller can decide whether that is an error.
static int LU_Decompose(LU *lu, Matrix *m) {
	REBLEN n = m->rows, i, j, k, p;
	double mag = 0.0, eps, piv, f, v;
 
	lu->n    = n;
	lu->sign = 1;
	lu->a    = (double*)malloc(sizeof(double) * n * n);
	lu->perm = (REBLEN*)malloc(sizeof(REBLEN) * n);
	if (!lu->a || !lu->perm) { LU_Free(lu); return 0; }
 
	#define LU_LOAD(T) do {                                    \
		T *q = (T*)m->data;                                    \
		for (i = 0; i < n * n; i++) lu->a[i] = (double)q[i];   \
	} while(0)
	VEC_DISPATCH(m->info, LU_LOAD);
	#undef LU_LOAD
 
	for (i = 0; i < n * n; i++) {
		v = DABS(lu->a[i]);
		if (v > mag) mag = v;
	}
	// Elimination leaves rounding noise where an exact zero was expected --
	// a singular 3x3 of small integers can end with a pivot near 1e-16 --
	// so the singularity test is relative to the largest input element.
	// An all-zero matrix gives eps == 0 and is caught by the <= below.
	eps = mag * 1E-12;
 
	for (i = 0; i < n; i++) lu->perm[i] = i;
 
	for (k = 0; k < n; k++) {
		p = k;
		for (i = k + 1; i < n; i++)
			if (DABS(lu->a[i*n+k]) > DABS(lu->a[p*n+k])) p = i;
 
		if (DABS(lu->a[p*n+k]) <= eps) { lu->sign = 0; return 1; }
 
		if (p != k) {
			for (j = 0; j < n; j++) {
				double t = lu->a[k*n+j];
				lu->a[k*n+j] = lu->a[p*n+j];
				lu->a[p*n+j] = t;
			}
			{ REBLEN t = lu->perm[k]; lu->perm[k] = lu->perm[p]; lu->perm[p] = t; }
			lu->sign = -lu->sign;
		}
 
		piv = lu->a[k*n+k];
		for (i = k + 1; i < n; i++) {
			f = lu->a[i*n+k] / piv;
			lu->a[i*n+k] = f;                       // keep the multiplier (L)
			for (j = k + 1; j < n; j++)
				lu->a[i*n+j] -= f * lu->a[k*n+j];
		}
	}
	return 1;
}
 
// Solves A x = b for one right-hand side, applying the stored permutation.
// `x` must not alias `b`.
static void LU_Solve(LU *lu, const double *b, double *x) {
	REBLEN n = lu->n, i, j;
	double s;
 
	for (i = 0; i < n; i++) {                       // forward, unit diagonal
		s = b[lu->perm[i]];
		for (j = 0; j < i; j++) s -= lu->a[i*n+j] * x[j];
		x[i] = s;
	}
	for (i = n; i-- > 0; ) {                        // back substitution
		s = x[i];
		for (j = i + 1; j < n; j++) s -= lu->a[i*n+j] * x[j];
		x[i] = s / lu->a[i*n+i];
	}
}
 
 
//== commands =================================================================
 
COMMAND cmd_matrix_determinant(RXIFRM *frm, void *ctx) {
	Matrix m;
	LU     lu = {0};
	double det;
	REBLEN k;
 
	if (!Matrix_Arg(&m, frm, 1)) RETURN_ERROR(ERR_NO_SHAPE);
	if (!IS_SQUARE(m))           RETURN_ERROR(ERR_NOT_SQUARE);
	if (!LU_Decompose(&lu, &m))  RETURN_ERROR(ERR_NO_RESULT);
 
	if (lu.sign == 0) {
		det = 0.0;                                  // singular, exactly zero
	} else {
		det = (double)lu.sign;
		for (k = 0; k < lu.n; k++) det *= lu.a[k*lu.n+k];
	}
	LU_Free(&lu);
 
	RXA_DEC64(frm, 1) = det;
	RXA_TYPE(frm, 1)  = RXT_DECIMAL;
	return RXR_VALUE;
}
 
COMMAND cmd_matrix_invert(RXIFRM *frm, void *ctx) {
	Matrix  m;
	LU      lu = {0};
	double *col, *x, *pd;
	REBYTE *dst;
	REBLEN  n, i, j;
 
	if (!Matrix_Arg(&m, frm, 1)) RETURN_ERROR(ERR_NO_SHAPE);
	if (!IS_SQUARE(m))           RETURN_ERROR(ERR_NOT_SQUARE);
	if (!LU_Decompose(&lu, &m))  RETURN_ERROR(ERR_NO_RESULT);
	if (lu.sign == 0) { LU_Free(&lu); RETURN_ERROR(ERR_SINGULAR); }
 
	n   = lu.n;
	col = (double*)malloc(sizeof(double) * n * 2);
	if (!col) { LU_Free(&lu); RETURN_ERROR(ERR_NO_RESULT); }
	x = col + n;
 
	// Allocating the result overwrites argument 1, but the input is already
	// copied into the LU workspace by this point.
	if (!(dst = Matrix_Result_Type(frm, VTSF64, n, n))) {
		free(col); LU_Free(&lu); RETURN_ERROR(ERR_NO_RESULT);
	}
	pd = (double*)dst;
 
	// One solve per unit column gives one column of the inverse.
	for (j = 0; j < n; j++) {
		for (i = 0; i < n; i++) col[i] = (i == j) ? 1.0 : 0.0;
		LU_Solve(&lu, col, x);
		for (i = 0; i < n; i++) pd[i*n+j] = x[i];
	}
 
	free(col);
	LU_Free(&lu);
	return RXR_VALUE;
}
 
COMMAND cmd_matrix_solve(RXIFRM *frm, void *ctx) {
	Matrix  a, b;
	LU      lu = {0};
	double *rhs, *x;
	REBYTE *dst;
	REBLEN  n, i;
 
	if (!Matrix_Arg(&a, frm, 1) || !Matrix_Arg(&b, frm, 2))
		RETURN_ERROR(ERR_NO_SHAPE);
	if (!IS_SQUARE(a))                    RETURN_ERROR(ERR_NOT_SQUARE);
	if (b.cols != 1 || b.rows != a.rows)  RETURN_ERROR(ERR_BAD_RHS);
	if (!LU_Decompose(&lu, &a))           RETURN_ERROR(ERR_NO_RESULT);
	if (lu.sign == 0) { LU_Free(&lu);     RETURN_ERROR(ERR_SINGULAR); }
 
	n   = lu.n;
	rhs = (double*)malloc(sizeof(double) * n * 2);
	if (!rhs) { LU_Free(&lu); RETURN_ERROR(ERR_NO_RESULT); }
	x = rhs + n;
 
	#define SOLVE_LOAD(T) do {                                 \
		T *q = (T*)b.data;                                     \
		for (i = 0; i < n; i++) rhs[i] = (double)q[i];         \
	} while(0)
	VEC_DISPATCH(b.info, SOLVE_LOAD);
	#undef SOLVE_LOAD
 
	LU_Solve(&lu, rhs, x);
 
	if (!(dst = Matrix_Result_Type(frm, VTSF64, 1, n))) {
		free(rhs); LU_Free(&lu); RETURN_ERROR(ERR_NO_RESULT);
	}
	for (i = 0; i < n; i++) ((double*)dst)[i] = x[i];
 
	free(rhs);
	LU_Free(&lu);
	return RXR_VALUE;
}
