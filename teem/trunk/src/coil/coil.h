/*
  teem: Gordon Kindlmann's research software
  Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998 University of Utah

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef COIL_HAS_BEEN_INCLUDED
#define COIL_HAS_BEEN_INCLUDED

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/ell.h>
#include <teem/nrrd.h>
#include <teem/gage.h>
#include <teem/ten.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COIL coilBiffKey

/*
******** coil_t
** 
** this is the very crude means by which you can control the type
** of values that coil works with: "float" or "double".  It is an
** unfortunate but greatly simplifying restriction that this type
** is used for all kinds of volumes, and all methods of filtering
**
** So: choose double by defining TEEM_COIL_TYPE_DOUBLE and float
** otherwise.
*/

#ifdef TEEM_COIL_TYPE_DOUBLE
typedef double coil_t;
#define coil_nrrdType nrrdTypeDouble
#define COIL_TYPE_FLOAT 0
#else
typedef float coil_t;
#define coil_nrrdType nrrdTypeFloat
#define COIL_TYPE_FLOAT 1
#endif

/*
******** #define COIL_PARMS_NUM
**
** maximum number of parameters that may be needed by any coil-driven
** filtering method
*/
#define COIL_PARMS_NUM 5

/*
******** coilMethodType* enum
**
** enumerated possibilities for different filtering methods
*/
enum {
  coilMethodTypeUnknown,            /* 0 */
  coilMethodTypeTesting,            /* 1: basically a no-op */
  coilMethodTypeIsotropic,          /* 2 */
  coilMethodTypeAnisotropic,        /* 3 */
  coilMethodTypeModifiedCurvature,  /* 4 */
  coilMethodTypeCurvatureFlow,      /* 5 */
  coilMethodTypeLast
};
#define COIL_METHOD_TYPE_MAX           5

/*
******** coilMethod struct
**
** someday, there will be total orthogonality between kind and method.
** until then, this will have only a portion of the things relating to
** running one method, regardless of kind
*/
typedef struct {
  char name[AIR_STRLEN_SMALL];
  int type;                         /* from coilMethodType* enum */
  int numParm;                      /* number of parameters we need */
} coilMethod;

/*
******** coilKindType* enum
**
** enumerated possibilities for different kinds
*/
enum {
  coilKindTypeUnknown,              /* 0 */
  coilKindTypeScalar,               /* 1 */
  coilKindType3Color,               /* 2 */
  coilKindType7Tensor,              /* 3 */
  coilKindTypeLast
};
#define COIL_KIND_TYPE_MAX             3

/*
******** coilKind struct
**
** yes, there is some redunancy with the gageKind, but the the main
** reason is that its significantly easier to implement meaningful
** per-sample filtering of various quantities, than it is to do
** gage-style convolution-based measurements at arbitrary locations.
** So, there will probably be significantly more coilKinds than
** there are gageKinds.  The two kind systems may play closer together
** at some point in the future where appropriate.
*/
typedef struct {
  char name[AIR_STRLEN_SMALL];      /* short identifying string for kind */
  int valLen;                       /* number of scalars per data point
				       1 for plain scalars (baseDim=0),
				       or something else (baseDim=1) */
                                    /* all the available methods */
  void (*filter[COIL_METHOD_TYPE_MAX+1])(coil_t *delta, coil_t *iv3,
					 double spacing[3],
					 double parm[COIL_PARMS_NUM]);
  void (*update)(coil_t *val, coil_t *delta); /* how to apply update */
} coilKind;

struct coilContext_t;

/*
******** coilTask
**
** passed to all worker threads
*/
typedef struct {
  struct coilContext_t *cctx;      /* parent's context */
  airThread *thread;               /* my thread */
  int threadIdx,                   /* which thread am I */
    startZ, endZ;                  /* my index range (inclusive) of slices */
  coil_t *iv3;                     /* cache of local values, ordering same
				      as in gage: any multiple values per
				      voxel are the slowest axis, not the
				      fastest */
  void *returnPtr;                 /* for airThreadJoin */
} coilTask;

/*
******** coilContext struct
**
** bag of stuff relating to filtering one volume
*/
typedef struct coilContext_t {
  /* ---------- input */
  const Nrrd *nin;                 /* input volume, converted to coil_t nvol */
  const coilKind *kind;            /* what kind of volume is nin */
  const coilMethod *method;        /* what method of filtering to use */
  int radius,                      /* how big a neighborhood to look at when
				      doing filtering (use 1 for 3x3x3 size) */
    numThreads,                    /* number of threads to enlist */
    verbose;                       /* blah blah blah */
  double parm[COIL_PARMS_NUM];     /* all the parameters used to control the 
				      action of the filtering.  The timestep is
				      probably the first value. */
  /* ---------- internal */
  int size[3];                     /* size of volume */
  double spacing[3];               /* sample spacings we'll use- we perhaps 
				      should be using a gageShape, but this is
				      actually all we really need... */
  Nrrd *nvol;                      /* an interleaved volume of (1st) the last
				      filtering result, and (2nd) the update
				      values from the current iteration */
  int finished;                    /* used to signal all threads to return */
  coilTask **task;                 /* dynamically allocated array of tasks */
  airThreadBarrier *filterBarrier, /* for when filtering is done, and the
				      "update" values have been set */
    *updateBarrier,                /* after the update values have been
				      applied to current values */
    *finishBarrier;                /* so that thread 0 can see if filtering
				      should go onward, and set "finished" */
} coilContext;

/* defaultsCoil.c */
TEEM_API const char *coilBiffKey;
TEEM_API int coilDefaultRadius;

/* enumsCoil.c */
TEEM_API airEnum *coilMethodType;
TEEM_API airEnum *coilKindType;

/* scalarCoil.c */
TEEM_API const coilKind *coilKindScalar;

/* tensorCoil.c */
TEEM_API const coilKind *coilKindTensor;

/* realmethods.c */
TEEM_API const coilMethod *coilMethodTesting;
TEEM_API const coilMethod *coilMethodIsotropic;
TEEM_API const coilMethod *coilMethodArray[COIL_METHOD_TYPE_MAX+1];

/* methodsCoil.c (sorry, confusing name!) */
TEEM_API coilContext *coilContextNew();
TEEM_API int coilVolumeCheck(const Nrrd *nin, const coilKind *kind);
TEEM_API int coilContextAllSet(coilContext *cctx, const Nrrd *nin,
			       const coilKind *kind, const coilMethod *method,
			       int radius, int numThreads, int verbose,
			       double parm[COIL_PARMS_NUM]);
TEEM_API int coilOutputGet(Nrrd *nout, coilContext *cctx);
TEEM_API coilContext *coilContextNix(coilContext *cctx);

/* coreCoil.c */
TEEM_API int coilStart(coilContext *cctx);
TEEM_API int coilIterate(coilContext *cctx, int numIterations);
TEEM_API int coilFinish(coilContext *cctx);

#ifdef __cplusplus
}
#endif

#endif /* COIL_HAS_BEEN_INCLUDED */
