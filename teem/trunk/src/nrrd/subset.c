/*
  teem: Gordon Kindlmann's research software
  Copyright (C) 2002, 2001, 2000, 1999, 1998 University of Utah

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


#include "nrrd.h"

/*
******** nrrdSample_nva()
**
** given coordinates within a nrrd, copies the 
** single element into given *val
*/
int
nrrdSample_nva(void *val, Nrrd *nrrd, int *coord) {
  char me[]="nrrdSample_nva", err[NRRD_STRLEN_MED];
  int typeSize, size[NRRD_DIM_MAX], d;
  nrrdBigInt I;
  
  if (!(nrrd && coord && val)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  /* this shouldn't actually be necessary ... */
  if (!nrrdElementSize(nrrd)) {
    sprintf(err, "%s: nrrd reports zero element size!", me);
    biffAdd(NRRD, err); return 1;
  }
  
  typeSize = nrrdElementSize(nrrd);
  nrrdAxesGet_nva(nrrd, nrrdAxesInfoSize, size);
  for (d=0; d<=nrrd->dim-1; d++) {
    if (!(AIR_INSIDE(0, coord[d], size[d]-1))) {
      sprintf(err, "%s: coordinate %d on axis %d out of bounds (0 to %d)", 
	      me, coord[d], d, size[d]-1);
      biffAdd(NRRD, err); return 1;
    }
  }

  NRRD_COORD_INDEX(I, coord, size, nrrd->dim);

  memcpy(val, (char*)(nrrd->data) + I*typeSize, typeSize);
  return 0;
}

/*
******** nrrdSample()
**
** var-args version of nrrdSample_nva()
*/
int
nrrdSample(void *val, Nrrd *nrrd, ...) {
  char me[]="nrrdSample", err[NRRD_STRLEN_MED];
  int d, coord[NRRD_DIM_MAX];
  va_list ap;
  
  if (!(nrrd && val)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }

  va_start(ap, nrrd);
  for (d=0; d<=nrrd->dim-1; d++) {
    coord[d] = va_arg(ap, int);
  }
  va_end(ap);
  
  if (nrrdSample_nva(val, nrrd, coord)) {
    sprintf(err, "%s:", me);
    biffAdd(NRRD, err); return 1;
  }
  return 0;
}

/*
******** nrrdSlice()
**
** slices a nrrd along a given axis, at a given position.
**
** will allocate memory for the new slice only if NULL==nout->data,
** otherwise assumes that the pointer there is pointing to something
** big enough to hold the slice
** 
** This is a newer version of the procedure, which is simpler, faster,
** and requires less memory overhead than the first one.  It is based
** on the observation that any slice is a periodic square-wave pattern
** in the original data (viewed as a one- dimensional array).  The
** characteristics of that periodic pattern are how far from the
** beginning it starts (offset), the length of the "on" part (length),
** the period (period), and the number of periods (numper). 
*/
int
nrrdSlice(Nrrd *nout, Nrrd *nin, int axis, int pos) {
  char me[]="nrrdSlice", err[NRRD_STRLEN_MED];
  nrrdBigInt 
    I, 
    rowLen,                  /* length of segment */
    colStep,                 /* distance between start of each segment */
    colLen;                  /* number of periods */
  int i, outdim, map[NRRD_DIM_MAX], szOut[NRRD_DIM_MAX];
  char *src, *dest;

  if (!(nin && nout)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  if (1 == nin->dim) {
    sprintf(err, "%s: can't slice a 1-D nrrd; use nrrd{I,F,D}Lookup[]", me);
    biffAdd(NRRD, err); return 1;
  }
  if (!(AIR_INSIDE(0, axis, nin->dim-1))) {
    sprintf(err, "%s: slice axis %d out of bounds (0 to %d)", 
	    me, axis, nin->dim-1);
    biffAdd(NRRD, err); return 1;
  }
  if (!(AIR_INSIDE(0, pos, nin->axis[axis].size-1) )) {
    sprintf(err, "%s: position %d out of bounds (0 to %d)", 
	    me, pos, nin->axis[axis].size-1);
    biffAdd(NRRD, err); return 1;
  }
  /* this shouldn't actually be necessary ... */
  if (!nrrdElementSize(nin)) {
    sprintf(err, "%s: nrrd reports zero element size!", me);
    biffAdd(NRRD, err); return 1;
  }

  /* set up control variables */
  rowLen = colLen = 1;
  for (i=0; i<=nin->dim-1; i++) {
    if (i < axis) {
      rowLen *= nin->axis[i].size;
    }
    else if (i > axis) {
      colLen *= nin->axis[i].size;
    }
  }
  rowLen *= nrrdElementSize(nin);
  colStep = rowLen*nin->axis[axis].size;

  outdim = nin->dim-1;
  for (i=0; i<=outdim-1; i++) {
    map[i] = i + (i >= axis);
    szOut[i] = nin->axis[map[i]].size;
  }
  nout->blockSize = nin->blockSize;
  if (nrrdMaybeAlloc_nva(nout, nin->type, outdim, szOut)) {
    sprintf(err, "%s: failed to create slice", me);
    biffAdd(NRRD, err); return 1;
  }

  /* the skinny */
  src = nin->data;
  dest = nout->data;
  src += rowLen*pos;
  for (I=0; I<colLen; I++) {
    /* HEY: replace with AIR_MEMCPY() or similar, when applicable */
    memcpy(dest, src, rowLen);
    src += colStep;
    dest += rowLen;
  }

  /* copy the peripheral information */
  if (nrrdAxesCopy(nout, nin, map, NRRD_AXESINFO_NONE)) {
    sprintf(err, "%s:", me);
    biffAdd(NRRD, err); return 1;
  }
  nout->content = airFree(nout->content);
  if (nin->content) {
    nout->content = calloc(strlen("slice(,,)")
			   + strlen(nin->content)
			   + 11
			   + 11
			   + 1, sizeof(char));
    if (nout->content) {
      sprintf(nout->content, "slice(%s,%d,%d)", nin->content, axis, pos);
    }
    else {
      sprintf(err, "%s: couldn't alloc content string", me);
      biffAdd(NRRD, err); return 1;
    }
  }
  nout->min = nout->max = AIR_NAN;
  nout->oldMin = nout->oldMax = AIR_NAN;
  /* leave comments alone */

  return(0);
}

/*
******** nrrdCrop()
**
** select some sub-volume inside a given nrrd, producing an output
** nrrd with the same dimensions, but with equal or smaller sizes
** along each axis.
*/
int
nrrdCrop(Nrrd *nout, Nrrd *nin, int *min, int *max) {
  char me[]="nrrdCrop", err[NRRD_STRLEN_MED], buff[NRRD_STRLEN_SMALL];
  int d, dim,
    lineSize,                /* #bytes in one scanline to be copied */
    typeSize,                /* size of data type */
    cIn[NRRD_DIM_MAX],       /* coords for line start, in input */
    cOut[NRRD_DIM_MAX],      /* coords for line start, in output */
    szIn[NRRD_DIM_MAX],
    szOut[NRRD_DIM_MAX];
  nrrdBigInt I,
    idxIn, idxOut,           /* linear indices for input and output */
    numLines;                /* number of scanlines in output nrrd */
  char *dataIn, *dataOut;

  /* errors */
  if (!(nout && nin && min && max)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  dim = nin->dim;
  for (d=0; d<=dim-1; d++) {
    if (!(min[d] <= max[d])) {
      sprintf(err, "%s: axis %d min (%d) not <= max (%d)", 
	      me, d, min[d], max[d]);
      biffAdd(NRRD, err); return 1;
    }
    if (!(AIR_INSIDE(0, min[d], nin->axis[d].size-1) &&
	  AIR_INSIDE(0, max[d], nin->axis[d].size-1))) {
      sprintf(err, "%s: axis %d min (%d) or max (%d) out of bounds [0,%d]",
	      me, d, min[d], max[d], nin->axis[d].size-1);
      biffAdd(NRRD, err); return 1;
    }
  }
  /* this shouldn't actually be necessary ... */
  if (!nrrdElementSize(nin)) {
    sprintf(err, "%s: nrrd reports zero element size!", me);
    biffAdd(NRRD, err); return 1;
  }

  /* allocate */
  nrrdAxesGet_nva(nin, nrrdAxesInfoSize, szIn);
  numLines = 1;
  for (d=0; d<=dim-1; d++) {
    szOut[d] = max[d] - min[d] + 1;
    if (d)
      numLines *= szOut[d];
  }
  nout->blockSize = nin->blockSize;
  if (nrrdMaybeAlloc_nva(nout, nin->type, dim, szOut)) {
    sprintf(err, "%s:", me);
    biffAdd(NRRD, err); return 1;
  }
  lineSize = szOut[0]*nrrdElementSize(nin);
  
  /* the skinny */
  typeSize = nrrdElementSize(nin);
  dataIn = nin->data;
  dataOut = nout->data;
  memset(cOut, 0, NRRD_DIM_MAX*sizeof(int));
  /*
  printf("!%s: dim = %d\n", me, dim);
  printf("!%s: min  = %d %d %d\n", me, min[0], min[1], min[2]);
  printf("!%s: szIn = %d %d %d\n", me, szIn[0], szIn[1], szIn[2]);
  printf("!%s: szOut = %d %d %d\n", me, szOut[0], szOut[1], szOut[2]);
  printf("!%s: lineSize = %d\n", me, lineSize);
  printf("!%s: typeSize = %d\n", me, typeSize);
  printf("!%s: numLines = %d\n", me, (int)numLines);
  */
  for (I=0; I<=numLines-1; I++) {
    for (d=0; d<=dim-1; d++)
      cIn[d] = cOut[d] + min[d];
    NRRD_COORD_INDEX(idxOut, cOut, szOut, dim);
    NRRD_COORD_INDEX(idxIn, cIn, szIn, dim);
    /*
    printf("!%s: %5d: cOut=(%3d,%3d,%3d) --> idxOut = %5d\n",
	   me, (int)I, cOut[0], cOut[1], cOut[2], (int)idxOut);
    printf("!%s: %5d:  cIn=(%3d,%3d,%3d) -->  idxIn = %5d\n",
	   me, (int)I, cIn[0], cIn[1], cIn[2], (int)idxIn);
    */
    memcpy(dataOut + idxOut*typeSize, dataIn + idxIn*typeSize, lineSize);
    /* the lowest coordinate in cOut[] will stay zero, since we are 
       copying one (1-D) scanline at a time */
    NRRD_COORD_INCR(cOut, szOut, dim, 1);
  }
  if (nrrdAxesCopy(nout, nin, NULL, (NRRD_AXESINFO_SIZE
				     | NRRD_AXESINFO_AMINMAX ))) {
    sprintf(err, "%s:", me);
    biffAdd(NRRD, err); return 1;
  }
  for (d=0; d<=dim-1; d++) {
    nrrdAxisPosRange(&(nout->axis[d].min), &(nout->axis[d].max),
		     nin, d, min[d], max[d]);
  }
  nout->content = airFree(nout->content);
  if (nin->content) {
    nout->content = calloc(strlen("crop(,)")
			   + strlen(nin->content)
			   + dim*(strlen("[,]x") + 2*11)
			   + 1, sizeof(char));
    if (nout->content) {
      sprintf(nout->content, "crop(%s,", nin->content);
      for (d=0; d<=dim-1; d++) {
	sprintf(buff, "%s[%d,%d]", (d ? "x" : ""), min[d], max[d]);
	strcat(nout->content, buff);
      }
      sprintf(buff, ")");
      strcat(nout->content, buff);    
    }
    else {
      sprintf(err, "%s: couldn't alloc content string", me);
      biffAdd(NRRD, err); return 1;
    }
  }
  nout->min = nout->max = AIR_NAN;
  nout->oldMin = nout->oldMax = AIR_NAN;
  /* leave comments alone */

  return 0;
}

/*
******** nrrdPad()
**
** strictly for padding
*/
int
nrrdPad(Nrrd *nout, Nrrd *nin, int *min, int *max, int boundary, ...) {
  char me[]="nrrdPad", err[NRRD_STRLEN_MED], buff[NRRD_STRLEN_SMALL];
  double padValue=AIR_NAN;
  int d, outside, dim, typeSize,
    cIn[NRRD_DIM_MAX],       /* coords for line start, in input */
    cOut[NRRD_DIM_MAX],      /* coords for line start, in output */
    szIn[NRRD_DIM_MAX],
    szOut[NRRD_DIM_MAX];
  nrrdBigInt
    idxIn, idxOut,           /* linear indices for input and output */
    numOut;                  /* number of elements in output nrrd */
  va_list ap;
  char *dataIn, *dataOut;
  
  if (!(nout && nin && min && max)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(NRRD, err); return 1;
  }
  if (!AIR_BETWEEN(nrrdBoundaryUnknown, boundary, nrrdBoundaryLast)) {
    sprintf(err, "%s: boundary behavior %d invalid", me, boundary);
    biffAdd(NRRD, err); return 1;
  }
  if (nrrdBoundaryWeight == boundary) {
    sprintf(err, "%s: boundary strategy %s not applicable here", me,
	    nrrdEnumValToStr(nrrdEnumBoundary, boundary));
    biffAdd(NRRD, err); return 1;
  }
  if (nrrdTypeBlock == nin->type && nrrdBoundaryPad == boundary) {
    sprintf(err, "%s: with nrrd type %s, boundary %s not valid", me,
	    nrrdEnumValToStr(nrrdEnumType, nrrdTypeBlock),
	    nrrdEnumValToStr(nrrdEnumBoundary, nrrdBoundaryPad));
    biffAdd(NRRD, err); return 1;
  }
  if (nrrdBoundaryPad == boundary) {
    va_start(ap, boundary);
    padValue = va_arg(ap, double);
    va_end(ap);
  }
  switch(boundary) {
  case nrrdBoundaryPad:
  case nrrdBoundaryBleed:
  case nrrdBoundaryWrap:
    break;
  default:
    fprintf(stderr, "%s: PANIC: boundary %d unimplemented\n", 
	    me, boundary); exit(1); break;
  }
  /*
  printf("!%s: boundary = %d, padValue = %g\n", me, boundary, padValue);
  */

  dim = nin->dim;
  nrrdAxesGet_nva(nin, nrrdAxesInfoSize, szIn);
  for (d=0; d<=dim-1; d++) {
    if (!(min[d] <= 0)) {
      sprintf(err, "%s: axis %d min (%d) not <= 0", 
	      me, d, min[d]);
      biffAdd(NRRD, err); return 1;
    }
    if (!(max[d] >= szIn[d]-1)) {
      sprintf(err, "%s: axis %d max (%d) not >= size-1 (%d)", 
	      me, d, max[d], szIn[d]-1);
      biffAdd(NRRD, err); return 1;
    }
  }
  /* this shouldn't actually be necessary ... */
  if (!nrrdElementSize(nin)) {
    sprintf(err, "%s: nrrd reports zero element size!", me);
    biffAdd(NRRD, err); return 1;
  }

  /* allocate */
  numOut = 1;
  for (d=0; d<=dim-1; d++) {
    numOut *= (szOut[d] = -min[d] + max[d] + 1);
  }
  nout->blockSize = nin->blockSize;
  if (nrrdMaybeAlloc_nva(nout, nin->type, dim, szOut)) {
    sprintf(err, "%s:", me);
    biffAdd(NRRD, err); return 1;
  }
  
  /* the skinny */
  typeSize = nrrdElementSize(nin);
  dataIn = nin->data;
  dataOut = nout->data;
  memset(cOut, 0, NRRD_DIM_MAX*sizeof(int));
  for (idxOut=0; idxOut<=numOut-1; idxOut++) {
    outside = 0;
    for (d=0; d<=dim-1; d++) {
      cIn[d] = cOut[d] + min[d];
      switch(boundary) {
      case nrrdBoundaryPad:
      case nrrdBoundaryBleed:
	if (!AIR_INSIDE(0, cIn[d], szIn[d]-1)) {
	  cIn[d] = AIR_CLAMP(0, cIn[d], szIn[d]-1);
	  outside = 1;
	}
	break;
      case nrrdBoundaryWrap:
	if (!AIR_INSIDE(0, cIn[d], szIn[d]-1)) {
	  cIn[d] = AIR_MOD(cIn[d], szIn[d]);
	  outside = 1;
	}
	break;
      }
    }
    NRRD_COORD_INDEX(idxIn, cIn, szIn, dim);
    if (!outside) {
      /* the cIn coords are within the input nrrd: do memcpy() of whole
	 1-D scanline, then artificially bump for-loop to the end of
	 the scanline */
      memcpy(dataOut + idxOut*typeSize, dataIn + idxIn*typeSize,
	     szIn[0]*typeSize);
      idxOut += nin->axis[0].size-1;
      cOut[0] += nin->axis[0].size-1;
    }
    else {
      /* we copy only a single value */
      if (nrrdBoundaryPad == boundary) {
	nrrdDInsert[nout->type](dataOut, idxOut, padValue);
      }
      else {
	memcpy(dataOut + idxOut*typeSize, dataIn + idxIn*typeSize, typeSize);
      }
    }
    NRRD_COORD_INCR(cOut, szOut, dim, 0);
  }
  if (nrrdAxesCopy(nout, nin, NULL, (NRRD_AXESINFO_SIZE
				     | NRRD_AXESINFO_AMINMAX ))) {
    sprintf(err, "%s:", me);
    biffAdd(NRRD, err); return 1;
  }
  for (d=0; d<=dim-1; d++) {
    nrrdAxisPosRange(&(nout->axis[d].min), &(nout->axis[d].max),
		     nin, d, min[d], max[d]);
  }
  nout->content = airFree(nout->content);
  if (nin->content) {
    nout->content = calloc(strlen("pad(,)")
			   + strlen(nin->content)
			   + dim*(strlen("[,]x") + 2*11)
			   + 1, sizeof(char));
    if (nout->content) {
      sprintf(nout->content, "pad(%s,", nin->content);
      for (d=0; d<=dim-1; d++) {
	sprintf(buff, "%s[%d,%d]", (d ? "x" : ""), min[d], max[d]);
	strcat(nout->content, buff);
      }
      sprintf(buff, ")");
      strcat(nout->content, buff);    
    }
    else {
      sprintf(err, "%s: couldn't alloc content string", me);
      biffAdd(NRRD, err); return 1;
    }
  }
  nout->min = nout->max = AIR_NAN;
  nout->oldMin = nout->oldMax = AIR_NAN;
  /* leave comments alone */

  return 0;
}
