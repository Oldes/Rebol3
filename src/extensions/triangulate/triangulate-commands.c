//
// Project: Rebol/Triangulate extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Command implementations of the Triangle binding.
//
// One function per command; the enum, the declarations, the dispatch table
// and the `_init` handler are all generated from triangulate.reb.
//

#include "gen-triangulate.h"
#include "triangulate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The `io` word list is resolved once by the generated `_init` handler;
// W_TRIANGULATE_IO_* are indices into it, so this yields the global word id
// which RL_GET_FIELD and RL_SET_FIELD expect.
#define IO_WORD(name)   Triangulate_io_words[W_TRIANGULATE_IO_##name]

#define SAFE_FREE(m)    do { if (m) { free(m); (m) = NULL; } } while (0)

// Result of Read_Vector below.
enum {
	VECT_INVALID = -1,   // the field is set, but not to a usable vector
	VECT_ABSENT  =  0,   // the object has no such field, or it holds none
	VECT_OK      =  1
};

static const REBYTE *ERR_POINTS      = (const REBYTE*)"Points must be defined using a 64bit decimal vector, like #(f64! [0.0 0.0 1.0 0.0 ...])!";
static const REBYTE *ERR_POINTS_N    = (const REBYTE*)"Points must hold an X/Y pair for each point and at least 3 points (6 values)!";
static const REBYTE *ERR_ATTRIBUTES  = (const REBYTE*)"Attributes must be defined using a 64bit decimal vector!";
static const REBYTE *ERR_ATTRIBUTES_N= (const REBYTE*)"Invalid number of attributes (must be a multiple of the number of points)!";
static const REBYTE *ERR_MARKERS     = (const REBYTE*)"Markers must be defined using a 32bit integer vector, like #(i32! [1 1 0 ...])!";
static const REBYTE *ERR_MARKERS_N   = (const REBYTE*)"Invalid number of markers (must be the same as the number of points)!";
static const REBYTE *ERR_REGIONS     = (const REBYTE*)"Regions must be defined using a 64bit decimal vector!";
static const REBYTE *ERR_REGIONS_N   = (const REBYTE*)"Invalid number of regions (each region is defined using 4 numbers)!";
static const REBYTE *ERR_NO_VECTOR   = (const REBYTE*)"Failed to allocate the result vector!";


//== vector conversion ========================================================

// Reads an optional vector field of the input object.
//
// The vector is used in place - Triangle only reads the input lists - so the
// data pointer stays valid for the duration of the command and nothing is
// copied. A vector may be offset (`skip`), hence the index arithmetic.
static int Read_Vector(REBSER *obj, u32 word, REBFLG want_float, void **data, REBCNT *len) {
	RXIARG  arg;
	REBSER *ser;
	REBCNT  type, info, bits, index, tail;

	*data = NULL;
	*len  = 0;

	type = RL_GET_FIELD(obj, word, &arg);
	if (type == 0 || type == RXT_NONE) return VECT_ABSENT;
	if (type != RXT_VECTOR) return VECT_INVALID;

	info = arg.vector.info;
	bits = RXI_VECTOR_BITS(info);

	// Triangle's REAL is a double and its index lists are plain ints, so
	// only these two element types can be handed over without a conversion.
	if (want_float) {
		if (!RXI_VECTOR_FLOAT(info) || bits != 64) return VECT_INVALID;
	} else {
		if ( RXI_VECTOR_FLOAT(info) || bits != 32) return VECT_INVALID;
	}

	ser   = (REBSER*)arg.vector.series;
	index = arg.vector.index;
	tail  = (REBCNT)SERIES_TAIL(ser);
	if (index >= tail) return VECT_ABSENT;   // an empty vector is like none

	*len  = tail - index;
	*data = SERIES_DATA(ser) + ((REBUPT)index * (bits >> 3));
	return VECT_OK;
}

// Stores a result vector into a field of the output object.
//
// Fields which the output object does not declare are skipped - that is how
// the caller says which parts of the result it is interested in.
//
// Returns FALSE only when the vector could not be made; an absent field or an
// empty result is not an error.
static REBOOL Write_Vector(REBSER *obj, u32 word, REBCNT vtype, const void *src, REBCNT count) {
	RXIARG arg;

	if (RL_GET_FIELD(obj, word, &arg) == 0) return TRUE;
	if (src == NULL || count == 0) return TRUE;

	if (!RL_MAKE_VECTOR(&arg, vtype, (REBINT)count, 1)) return FALSE;

	// Until the RXIARG reaches Rebol nothing references the new series, so
	// the copy and the field assignment must follow immediately - no call
	// which may allocate is allowed in between.
	memcpy(SERIES_DATA((REBSER*)arg.vector.series), src, (size_t)count * VECT_WIDE(vtype));
	RL_SET_FIELD(obj, word, arg, RXT_VECTOR);
	return TRUE;
}


//== reporting ================================================================
//
// Prints the input or the output, like Triangle's own `report()` does.

static void Do_Report(
	struct triangulateio *io,
	int markers,
	int reporttriangles,
	int reportneighbors,
	int reportsegments,
	int reportedges,
	int reportnorms)
{
	int i, j;

	if (!io->pointmarkerlist) markers = 0;

	if (io->pointlist) {
		for (i = 0; i < io->numberofpoints; i++) {
			printf("Point %4d:", i);
			for (j = 0; j < 2; j++) {
				printf("  %.6g", io->pointlist[i * 2 + j]);
			}
			if (io->numberofpointattributes > 0) {
				printf("   attributes");
			}
			for (j = 0; j < io->numberofpointattributes; j++) {
				printf("  %.6g",
					io->pointattributelist[i * io->numberofpointattributes + j]);
			}
			if (markers && io->pointmarkerlist != NULL) {
				printf("   marker %d\n", io->pointmarkerlist[i]);
			} else {
				printf("\n");
			}
		}
	}
	printf("\n");

	if ((reporttriangles || reportneighbors) && io->trianglelist && io->numberoftriangleattributes) {
		for (i = 0; i < io->numberoftriangles; i++) {
			if (reporttriangles) {
				printf("Triangle %4d points:", i);
				for (j = 0; j < io->numberofcorners; j++) {
					printf("  %4d", io->trianglelist[i * io->numberofcorners + j]);
				}
				if (io->numberoftriangleattributes > 0) {
					printf("   attributes");
				}
				for (j = 0; j < io->numberoftriangleattributes; j++) {
					printf("  %.6g", io->triangleattributelist[i *
							io->numberoftriangleattributes + j]);
				}
				printf("\n");
			}
			if (reportneighbors && io->neighborlist != NULL) {
				printf("Triangle %4d neighbors:", i);
				for (j = 0; j < 3; j++) {
					printf("  %4d", io->neighborlist[i * 3 + j]);
				}
				printf("\n");
			}
		}
		printf("\n");
	}

	if (reportsegments && io->segmentlist) {
		for (i = 0; i < io->numberofsegments; i++) {
			printf("Segment %4d points:", i);
			for (j = 0; j < 2; j++) {
				printf("  %4d", io->segmentlist[i * 2 + j]);
			}
			if (markers && io->segmentmarkerlist) {
				printf("   marker %d\n", io->segmentmarkerlist[i]);
			} else {
				printf("\n");
			}
		}
		printf("\n");
	}

	if (reportedges && io->edgelist) {
		for (i = 0; i < io->numberofedges; i++) {
			printf("Edge %4d points:", i);
			for (j = 0; j < 2; j++) {
				printf("  %4d", io->edgelist[i * 2 + j]);
			}
			if (reportnorms && io->normlist && (io->edgelist[i * 2 + 1] == -1)) {
				for (j = 0; j < 2; j++) {
					printf("  %.6g", io->normlist[i * 2 + j]);
				}
			}
			if (markers && io->edgemarkerlist) {
				printf("   marker %d\n", io->edgemarkerlist[i]);
			} else {
				printf("\n");
			}
		}
		printf("\n");
	}
}


//== triangulation ============================================================

COMMAND cmd_triangulate_triangulate(RXIFRM *frm, void *ctx) {
	struct triangulateio in, mid, vorout;
	REBSER *objIn  = (REBSER*)RXA_OBJECT(frm, 1);
	REBSER *objOut = (REBSER*)RXA_OBJECT(frm, 2);
	RXIARG  arg;
	REBCNT  type, len;
	void   *data;
	REBOOL  report = FALSE;
	const REBYTE *error = NULL;
	char    flags[12];

	memset(&in,     0, sizeof(struct triangulateio));
	memset(&mid,    0, sizeof(struct triangulateio));
	memset(&vorout, 0, sizeof(struct triangulateio));

	//-- input ---------------------------------------------------------------
	// Nothing is allocated yet, so these may return an error directly.

	type = RL_GET_FIELD(objIn, IO_WORD(REPORT), &arg);
	if (type == RXT_LOGIC && arg.int32a != 0) report = TRUE;

	if (VECT_OK != Read_Vector(objIn, IO_WORD(POINTS), TRUE, &data, &len))
		RETURN_ERROR(ERR_POINTS);
	// Triangle exits the whole process when it is given less than three
	// vertices, so that case is caught here instead.
	if (len < 6 || (len & 1)) RETURN_ERROR(ERR_POINTS_N);
	in.pointlist      = (REAL*)data;
	in.numberofpoints = (int)(len >> 1);

	switch (Read_Vector(objIn, IO_WORD(ATTRIBUTES), TRUE, &data, &len)) {
	case VECT_OK:
		if ((len % (REBCNT)in.numberofpoints) != 0) RETURN_ERROR(ERR_ATTRIBUTES_N);
		in.numberofpointattributes = (int)(len / (REBCNT)in.numberofpoints);
		in.pointattributelist      = (REAL*)data;
		break;
	case VECT_INVALID:
		RETURN_ERROR(ERR_ATTRIBUTES);
	}

	// Triangle reads the markers as plain ints, so this is a 32bit integer
	// vector - it used to be checked as a 64bit one and then read as int*.
	switch (Read_Vector(objIn, IO_WORD(MARKERS), FALSE, &data, &len)) {
	case VECT_OK:
		if (len != (REBCNT)in.numberofpoints) RETURN_ERROR(ERR_MARKERS_N);
		in.pointmarkerlist = (int*)data;
		break;
	case VECT_INVALID:
		RETURN_ERROR(ERR_MARKERS);
	}

	switch (Read_Vector(objIn, IO_WORD(REGIONS), TRUE, &data, &len)) {
	case VECT_OK:
		if ((len % 4) != 0) RETURN_ERROR(ERR_REGIONS_N);
		in.numberofregions = (int)(len / 4);
		in.regionlist      = (REAL*)data;
		break;
	case VECT_INVALID:
		RETURN_ERROR(ERR_REGIONS);
	}

	//-- triangulation -------------------------------------------------------
	/* Switches are chosen to read and write a PSLG (p), preserve the convex  */
	/* hull (c), number everything from zero (z), and produce an edge list    */
	/* (e), a Voronoi diagram (v) and no console output of Triangle's own (Q).*/

	flags[0] = 'p';
	flags[1] = 'c';
	flags[2] = 'z';
	flags[3] = 'e';
	flags[4] = 'v';
	flags[5] = 'Q';
	flags[6] = 'g';
	flags[7] = 0;

	triangulate(flags, &in, &mid, &vorout);

	if (report) {
		printf("Initial triangulation:\n\n");
		Do_Report(&mid, 1, 1, 1, 1, 1, 0);
		printf("Initial Voronoi diagram:\n\n");
		Do_Report(&vorout, 0, 0, 0, 0, 1, 1);
	}

	//-- output --------------------------------------------------------------
	// From here on Triangle has allocated the result lists, so failures must
	// go through `finish` to release them.

#define STORE(word, vtype, src, count) \
	if (!Write_Vector(objOut, IO_WORD(word), vtype, (src), (REBCNT)(count))) { \
		error = ERR_NO_VECTOR; goto finish; }

	if (mid.numberofpoints > 0) {
		STORE(POINTS,  VTSF64, mid.pointlist,       2 * mid.numberofpoints);
		STORE(MARKERS, VTSI32, mid.pointmarkerlist,     mid.numberofpoints);

		if (mid.numberofpointattributes > 0) {
			STORE(ATTRIBUTES, VTSF64, mid.pointattributelist,
				mid.numberofpointattributes * mid.numberofpoints);
		}
	}

	if (mid.numberofsegments > 0) {
		STORE(SEGMENTS,        VTSI32, mid.segmentlist,       2 * mid.numberofsegments);
		STORE(SEGMENT_MARKERS, VTSI32, mid.segmentmarkerlist,     mid.numberofsegments);
	}

	if (mid.numberofedges > 0) {
		STORE(EDGES, VTSI32, mid.edgelist, 2 * mid.numberofedges);
	}

	if (mid.numberoftriangles > 0) {
		// NOTE: this used to copy `mid.edgelist` - the corner indices of the
		// triangles are in `mid.trianglelist`. There are `numberofcorners`
		// of them per triangle (3, unless the `o2` switch is used).
		STORE(TRIANGLES, VTSI32, mid.trianglelist,
			mid.numberofcorners * mid.numberoftriangles);
	}

	if (vorout.numberofpoints > 0) {
		STORE(V_POINTS, VTSF64, vorout.pointlist, 2 * vorout.numberofpoints);

		if (vorout.numberofpointattributes > 0) {
			STORE(V_ATTRIBUTES, VTSF64, vorout.pointattributelist,
				vorout.numberofpointattributes * vorout.numberofpoints);
		}
	}

	if (vorout.numberofedges > 0) {
		STORE(V_EDGES, VTSI32, vorout.edgelist, 2 * vorout.numberofedges);
		// NOTE: the norms are REALs; the size of the copy used to be taken
		// from sizeof(int), so only half of them arrived.
		STORE(V_NORMS, VTSF64, vorout.normlist, 2 * vorout.numberofedges);
	}

#undef STORE

finish:
	/* Free the arrays allocated by Triangle. The input lists are owned by  */
	/* the Rebol vectors they were read from and must not be freed here.    */
	SAFE_FREE(mid.pointlist);
	SAFE_FREE(mid.pointattributelist);
	SAFE_FREE(mid.pointmarkerlist);
	SAFE_FREE(mid.trianglelist);
	SAFE_FREE(mid.triangleattributelist);
	SAFE_FREE(mid.trianglearealist);
	SAFE_FREE(mid.neighborlist);
	SAFE_FREE(mid.segmentlist);
	SAFE_FREE(mid.segmentmarkerlist);
	SAFE_FREE(mid.edgelist);
	SAFE_FREE(mid.edgemarkerlist);
	SAFE_FREE(vorout.pointlist);
	SAFE_FREE(vorout.pointattributelist);
	SAFE_FREE(vorout.edgelist);
	SAFE_FREE(vorout.normlist);

	if (error) RETURN_ERROR(error);

	// The input object is returned, like it always was.
	return RXR_VALUE;
}