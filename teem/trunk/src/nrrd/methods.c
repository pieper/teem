/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in
  compliance with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
  the License for the specific language governing rights and limitations
  under the License.

  The Original Source Code is "teem", released March 23, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1998 University
  of Utah. All Rights Reserved.
*/


#include "nrrd.h"
#include "private.h"

void
_nrrdIOInit(nrrdIO *io) {

  if (io) {
    strcpy(io->dir, "");
    strcpy(io->base, "");
    strcpy(io->line, "");
    io->pos = 0;
    io->dataFile = NULL;
    io->magic = nrrdMagicUnknown;
    io->format = nrrdFormatNRRD;       /* sensible default */
    io->encoding = nrrdEncodingRaw;    /* sensible default */
    io->endian = airEndianUnknown;
    io->lineSkip = 0;
    io->byteSkip = 0;
    io->bareTable = AIR_TRUE;
    io->seperateHeader = AIR_FALSE;
    memset(io->seen, 0, (NRRD_FIELD_MAX+1)*sizeof(int));
  }
}

nrrdIO *
nrrdIONew(void) {
  nrrdIO *io;
  
  io = calloc(1, sizeof(nrrdIO));
  if (io) {
    _nrrdIOInit(io);
  }
  return io;
}

nrrdIO *
nrrdIONix(nrrdIO *io) {

  return airFree(io);
}

void
_nrrdResampleInfoInit(nrrdResampleInfo *info) {
  int i, d;

  for (d=0; d<=NRRD_DIM_MAX-1; d++) {
    info->kernel[d] = NULL;
    info->samples[d] = 0;
    info->param[d][0] = 1.0;
    for (i=1; i<=NRRD_KERNEL_PARAMS_MAX-1; i++)
      info->param[d][i] = AIR_NAN;
    info->min[d] = info->max[d] = AIR_NAN;
  }
  info->type = nrrdTypeUnknown;
  /* HEY: these may or may not be the best choices for default values */
  info->renormalize = AIR_TRUE;
  info->boundary = nrrdBoundaryBleed;
  info->padValue = 0.0;
}

nrrdResampleInfo *
nrrdResampleInfoNew(void) {
  nrrdResampleInfo *info;

  info = (nrrdResampleInfo*)(calloc(1, sizeof(nrrdResampleInfo)));
  if (info) {
    _nrrdResampleInfoInit(info);
  }
  return info;
}

nrrdResampleInfo *
nrrdResampleInfoNix(nrrdResampleInfo *info) {
  
  return airFree(info);
}

/*
******* nrrdInit
**
** initializes a nrrd to default state.  Mostly just sets values to 
** -1, NaN, "", NULL, or Unknown
*/
void
nrrdInit(Nrrd *nrrd) {
  int i;

  if (nrrd) {
    nrrd->data = airFree(nrrd->data);
    nrrd->num = -1;
    nrrd->type = nrrdTypeUnknown;
    nrrd->dim = -1;
    
    for (i=0; i<=NRRD_DIM_MAX-1; i++) {
      _nrrdAxisInit(nrrd->axis + i);
    }
    
    nrrd->content = airFree(nrrd->content);
    nrrd->blockSize = -1;
    nrrd->min = nrrd->max = AIR_NAN;
    nrrd->oldMin = nrrd->oldMax = AIR_NAN;
    nrrd->ptr = NULL;
    
    nrrdCommentClear(nrrd);
  }
}

/*
******** nrrdNew()
**
** creates and initializes a Nrrd
**
** this does NOT use biff
*/
Nrrd *
nrrdNew(void) {
  Nrrd *nrrd;
  
  nrrd = (Nrrd*)(calloc(1, sizeof(Nrrd)));
  if (!nrrd)
    return NULL;
  nrrd->data = NULL;
  nrrd->content = NULL;
  nrrd->cmtArr = airArrayNew((void**)(&(nrrd->cmt)), NULL, 
			     sizeof(char *), NRRD_COMMENT_INCR);
  if (!nrrd->cmtArr)
    return NULL;
  nrrdInit(nrrd);
  return nrrd;
}

/*
******** nrrdNix()
**
** does nothing with the array, just does whatever is needed
** to free the nrrd itself
**
** returns NULL
**
** this does NOT use biff
*/
Nrrd *
nrrdNix(Nrrd *nrrd) {
  
  if (nrrd) {
    nrrd->content = airFree(nrrd->content);
    nrrdCommentClear(nrrd);
    nrrd = airFree(nrrd);
  }
  return NULL;
}

/*
******** nrrdEmpty()
**
** frees data inside nrrd AND resets all its state, so its the
** same as what comes from nrrdNew()
*/
Nrrd *
nrrdEmpty(Nrrd *nrrd) {
  
  if (nrrd) {
    nrrd->data = airFree(nrrd->data);
    nrrdInit(nrrd);
  }
  return nrrd;
}

/*
******** nrrdNuke()
**
** blows away the nrrd and everything inside
**
** always returns NULL
*/
Nrrd *
nrrdNuke(Nrrd *nrrd) {
  
  if (nrrd) {
    nrrdEmpty(nrrd);
    nrrdNix(nrrd);
  }
  return NULL;
}

/*
******** nrrdWrap()
**
** wraps a given Nrrd around a given array
**
** NOTE: does not touch any fields other than what the arguments imply
*/
Nrrd *
nrrdWrap(Nrrd *nrrd, void *data, nrrdBigInt num, int type, int dim) {

  if (nrrd) {
    nrrd->data = data;
    nrrd->num = num;
    nrrd->type = type;
    nrrd->dim = dim;
  }
  return nrrd;
}

/*
******** nrrdWrap_va()
**
** minimal var args wrapper around nrrdWrap
*/
Nrrd *
nrrdWrap_va(Nrrd *nrrd, void *data, int type, int dim, ...) {
  nrrdBigInt num;
  va_list ap;
  int d;
  
  if (!nrrd) {
    return NULL;
  }
  num = 1;
  va_start(ap, dim);
  for (d=0; d<=dim-1; d++) {
    nrrd->axis[d].size = va_arg(ap, int);
    num *= nrrd->axis[d].size;
  }
  return nrrdWrap(nrrd, data, num, type, dim);
}


/*
******** nrrdUnwrap()
**
** a wrapper around nrrdNix()
*/
Nrrd *
nrrdUnwrap(Nrrd *nrrd) {
  
  return nrrdNix(nrrd);
}

/*
******** nrrdAlloc()
**
** allocates data array and sets information
**
** This function will always allocate more memory (via calloc), but
** it will free() nrrd->data if it is non-NULL when passed in
**
** Note to Gordon: don't get clever and change ANY axis-specific
** information here.  It may be very conveniant to set that before
** nrrdAlloc() or nrrdMaybeAlloc()
**
** Note: This function DOES use biff
*/
int 
nrrdAlloc(Nrrd *nrrd, nrrdBigInt num, int type, int dim) {
  char err[NRRD_STRLEN_MED], me[] = "nrrdAlloc";

  if (!nrrd) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  if (!(num > 0)) {
    sprintf(err, "%s: invalid num (%d)", me, (int)num);
    biffAdd(NRRD, err); return 1;
  }
  if (!AIR_BETWEEN(nrrdTypeUnknown, type, nrrdTypeLast)) {
    sprintf(err, "%s: type (%d) is invalid", me, type);
    biffAdd(NRRD, err); return 1;
  }
  if (nrrdTypeBlock == type) {
    sprintf(err, "%s: sorry, can't deal with nrrdTypeBlock here", me);
    biffAdd(NRRD, err); return 1;
  }
  if (!AIR_INSIDE(1, dim, NRRD_DIM_MAX)) {
    sprintf(err, "%s: dim (%d) in invalid", me, dim);
    biffAdd(NRRD, err); return 1;
  }

  nrrd->data = airFree(nrrd->data);
  nrrd->data = calloc(num, nrrdTypeSize[type]);
  if (!(nrrd->data)) {
    sprintf(err, "%s: calloc(" NRRD_BIG_INT_PRINTF ",%d) failed", 
	    me, num, nrrdTypeSize[type]);
    biffAdd(NRRD, err); return 1 ;
  }
  nrrd->num = num;
  nrrd->type = type;
  nrrd->dim = dim;

  return 0;
}

/*
******** nrrdAlloc_va()
**
** Handy wrapper around nrrdAlloc, which takes, as its vararg list
** all the axes sizes, thereby calculating the total number.
*/
int 
nrrdAlloc_va(Nrrd *nrrd, int type, int dim, ...) {
  char me[]="nrrdAlloc_va", err[NRRD_STRLEN_MED];
  int d;
  nrrdBigInt num;
  va_list ap;
  
  if (!nrrd) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  num = 1;
  va_start(ap, dim);
  for (d=0; d<=dim-1; d++) {
    nrrd->axis[d].size = va_arg(ap, int);
    num *= nrrd->axis[d].size;
  }
  if (nrrdAlloc(nrrd, num, type, dim)) {
    sprintf(err, "%s: trouble", me);
    biffAdd(NRRD, err); return 1;
  }
  return 0;
}


/*
******** nrrdMaybeAlloc
**
** calls nrrdAlloc if the requested space is different than
** what is currently held
**
** Note to Gordon: don't get clever and reset any axis-specific
** information here.  It may be very conveniant to set that before
** nrrdAlloc() or nrrdMaybeAlloc()
**
*/
int
nrrdMaybeAlloc(Nrrd *nrrd, nrrdBigInt num, int type, int dim) {
  char err[NRRD_STRLEN_MED], me[]="nrrdMaybeAlloc";
  nrrdBigInt sizeWant, sizeHave;
  int need;
  
  if (!nrrd) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  if (!AIR_BETWEEN(nrrdTypeUnknown, type, nrrdTypeLast)) {
    sprintf(err, "%s: type (%d) is invalid", me, type);
    biffAdd(NRRD, err); return 1;
  }
  if (nrrdTypeBlock == type) {
    sprintf(err, "%s: sorry, can't deal with nrrdTypeBlock here", me);
    biffAdd(NRRD, err); return 1;
  }
  if (!(nrrd->data)) {
    need = 1;
  }
  else {
    sizeHave = nrrd->num * nrrdElementSize(nrrd);
    sizeWant = num * nrrdTypeSize[type];
    need = sizeHave != sizeWant;
  }
  if (need) {
    if (nrrdAlloc(nrrd, num, type, dim)) {
      sprintf(err, "%s: trouble", me);
      biffAdd(NRRD, err); return 1;
    }
  }
  return 0;
}

/*
******** nrrdMaybeAlloc_va()
**
** Handy wrapper around nrrdAlloc, which takes, as its vararg list
** all the axes sizes, thereby calculating the total number.
*/
int 
nrrdMaybeAlloc_va(Nrrd *nrrd, int type, int dim, ...) {
  char me[]="nrrdMaybeAlloc_va", err[NRRD_STRLEN_MED];
  int d;
  nrrdBigInt num;
  va_list ap;
  
  if (!nrrd) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  num = 1;
  va_start(ap, dim);
  for (d=0; d<=dim-1; d++) {
    nrrd->axis[d].size = va_arg(ap, int);
    num *= nrrd->axis[d].size;
  }
  if (nrrdMaybeAlloc(nrrd, num, type, dim)) {
    sprintf(err, "%s: trouble", me);
    biffAdd(NRRD, err); return 1;
  }
  return 0;
}

/*
******** nrrdCopy
**
** copy method for nrrds.  nout will end up as an exact copy of nin,
** except that nout->ptr is NULL.
*/
int
nrrdCopy(Nrrd *nout, Nrrd *nin) {
  char me[]="nrrdCopy", err[NRRD_STRLEN_MED];

  if (!(nin && nout)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  if (nout == nin) {
    /* I guess there's nothing to do! */
    return 0;
  }
  if (nrrdMaybeAlloc(nout, nin->num, nin->type, nin->dim)) {
    sprintf(err, "%s: couldn't allocate data", me);
    biffAdd(NRRD, err); return 1;
  }
  memcpy(nout->data, nin->data, nin->num*nrrdElementSize(nin));
  nrrdAxesCopy(nout, nin, NULL, NRRD_AXESINFO_NONE);

  nout->content = airStrdup(nin->content);
  if (nin->content && !nout->content) {
    sprintf(err, "%s: couldn't copy content", me);
    biffAdd(NRRD, err); return 1;
  }
  nout->blockSize = nin->blockSize;
  nout->min = nin->min;
  nout->max = nin->max;
  nout->oldMin = nin->oldMin;
  nout->oldMax = nin->oldMax;
  nout->ptr = NULL;
    
  if (nrrdCommentCopy(nout, nin, AIR_TRUE)) {
    sprintf(err, "%s: trouble copying comments", me);
    biffAdd(NRRD, err); return 1;
  }

  return 0;
}

/*
******** nrrdPPM()
**
** for making a nrrd suitable for holding PPM data
*/
int
nrrdPPM(Nrrd *ppm, int sx, int sy) {
  char err[NRRD_STRLEN_MED], me[] = "nrrdNewPPM";

  if (!(sx > 0 && sy > 0)) {
    sprintf(err, "%s: got invalid sizes (%d,%d)", me, sx, sy);
    biffAdd(NRRD, err);
    return 1;
  }
  if (nrrdAlloc_va(ppm, nrrdTypeUChar, 3, 3, sx, sy)) {
    sprintf(err, "%s: couldn't allocate %d x %d 24-bit image", me, sx, sy);
    biffAdd(NRRD, err);
    return 1;
  }
  return 0;
}

/*
******** nrrdPGM()
**
** for making a nrrd suitable for holding PGM data
*/
int
nrrdPGM(Nrrd *pgm, int sx, int sy) {
  char err[NRRD_STRLEN_MED], me[] = "nrrdNewPGM";

  if (!(sx > 0 && sy > 0)) {
    sprintf(err, "%s: got invalid sizes (%d,%d)", me, sx, sy);
    biffAdd(NRRD, err);
    return 1;
  }
  if (nrrdAlloc_va(pgm, nrrdTypeUChar, 2, sx, sy)) {
    sprintf(err, "%s: couldn't allocate %d x %d 8-bit image", me, sx, sy);
    biffAdd(NRRD, err);
    return 1;
  }
  return 0;
}

/*
******** nrrdTable()
**
** for making a nrrd suitable for holding table data
*/
int
nrrdTable(Nrrd *table, int sx, int sy) {
  char me[]="nrrdTable", err[NRRD_STRLEN_MED];

  if (!(sx > 0 && sy > 0)) {
    sprintf(err, "%s: got invalid sizes (%d,%d)", me, sx, sy);
    biffAdd(NRRD, err);
    return 1;
  }
  if (nrrdAlloc_va(table, nrrdTypeFloat, 2, sx, sy)) {
    sprintf(err, "%s: couldn't allocate %d x %d table of floats", me, sx, sy);
    biffAdd(NRRD, err);
    return 1;
  }
  return 0;
}
