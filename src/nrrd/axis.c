/*
  teem: Gordon Kindlmann's research software
  Copyright (C) 2005  Gordon Kindlmann
  Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998  University of Utah

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
#include "privateNrrd.h"

/* ------------------------------------------------------------ */

void
_nrrdAxisInfoInit(NrrdAxisInfo *axis) {
  int dd;
  
  if (axis) {
    axis->size = 0;
    axis->spacing = axis->thickness = AIR_NAN;
    axis->min = axis->max = AIR_NAN;
    for (dd=0; dd<NRRD_SPACE_DIM_MAX; dd++) {
      axis->spaceDirection[dd] = AIR_NAN;
    }
    axis->center = nrrdCenterUnknown;
    axis->kind = nrrdKindUnknown;
    axis->label = airFree(axis->label);
    axis->units = airFree(axis->units);
  }
}

void
_nrrdAxisInfoNewInit(NrrdAxisInfo *axis) {
  int dd;
  
  if (axis) {
    axis->label = NULL;
    axis->units = NULL;
    _nrrdAxisInfoInit(axis);
  }
}

/* ------------------------------------------------------------ */

/*
******** nrrdKindSize
**
** returns suggested size (length) of an axis with the given kind, or,
** 0 if there is no suggested size, or the kind was invalid
*/
int
nrrdKindSize(int kind) {
  char me[]="nrrdKindSize";
  int ret;
  
  if (!( AIR_IN_OP(nrrdKindUnknown, kind, nrrdKindLast) )) {
    /* they gave us invalid or unknown kind */
    return 0;
  }

  switch (kind) {
  case nrrdKindDomain:
  case nrrdKindSpace:
  case nrrdKindTime:
  case nrrdKindList:
    ret = 0;
    break;
  case nrrdKindStub:
  case nrrdKindScalar:
    ret = 1;
    break;
  case nrrdKindComplex:
  case nrrdKind2Vector:
    ret = 2;
    break;
  case nrrdKind3Color:
  case nrrdKind3Vector:
  case nrrdKind3Normal:
    ret = 3;
    break;
  case nrrdKind4Color:
  case nrrdKind4Vector:
    ret = 4;
    break;
  case nrrdKind2DSymMatrix:
    ret = 3;
    break;
  case nrrdKind2DMaskedSymMatrix:
    ret = 4;
    break;
  case nrrdKind2DMatrix:
    ret = 4;
    break;
  case nrrdKind2DMaskedMatrix:
    ret = 5;
    break;
  case nrrdKind3DSymMatrix:
    ret = 6;
    break;
  case nrrdKind3DMaskedSymMatrix:
    ret = 7;
    break;
  case nrrdKind3DMatrix:
    ret = 9;
    break;
  case nrrdKind3DMaskedMatrix:
    ret = 10;
    break;
  default:
    fprintf(stderr, "%s: PANIC: nrrdKind %d not implemented!\n", me, kind);
    exit(1);
  }

  return ret;
}

int
_nrrdKindAltered(int kindIn) {
  int kindOut;

  if (nrrdStateKindNoop) {
    kindOut = nrrdKindUnknown;
    /* HEY: setting the kindOut to unknown is arguably not a no-op.
       It is more like "conservative".  So maybe nrrdStateKindNoop
       should be renamed.  But nrrdStateKindConservative would imply
       that the kind is conserved, which is exactly NOT what we do ... */
  } else {
    if (nrrdKindDomain == kindIn
        || nrrdKindSpace == kindIn
        || nrrdKindTime == kindIn
        || nrrdKindList == kindIn) {
      /* HEY: shouldn't we disallow or at least warn when a "List"
         is being resampled? */
      kindOut = kindIn;
    } else {
      kindOut = nrrdKindUnknown;
    }
  }
  return kindOut;
}

/*
** _nrrdAxisInfoCopy
**
** HEY: we have a void return even though this function potentially
** involves calling airStrdup!!  
*/
void
_nrrdAxisInfoCopy(NrrdAxisInfo *dest, const NrrdAxisInfo *src, int bitflag) {
  int ii;

  if (!(NRRD_AXIS_INFO_SIZE_BIT & bitflag)) {
    dest->size = src->size;
  }
  if (!(NRRD_AXIS_INFO_SPACING_BIT & bitflag)) {
    dest->spacing = src->spacing;
  }
  if (!(NRRD_AXIS_INFO_THICKNESS_BIT & bitflag)) {
    dest->thickness = src->thickness;
  }
  if (!(NRRD_AXIS_INFO_MIN_BIT & bitflag)) {
    dest->min = src->min;
  }
  if (!(NRRD_AXIS_INFO_MAX_BIT & bitflag)) {
    dest->max = src->max;
  }
  if (!(NRRD_AXIS_INFO_SPACEDIRECTION_BIT & bitflag)) {
    for (ii=0; ii<NRRD_SPACE_DIM_MAX; ii++) {
      dest->spaceDirection[ii] = src->spaceDirection[ii];
    }
  }
  if (!(NRRD_AXIS_INFO_CENTER_BIT & bitflag)) {
    dest->center = src->center;
  }
  if (!(NRRD_AXIS_INFO_KIND_BIT & bitflag)) {
    dest->kind = src->kind;
  }
  if (!(NRRD_AXIS_INFO_LABEL_BIT & bitflag)) {
    if (dest->label != src->label) {
      dest->label = airFree(dest->label);
      dest->label = airStrdup(src->label);
    }
  }
  if (!(NRRD_AXIS_INFO_UNITS_BIT & bitflag)) {
    if (dest->units != src->units) {
      dest->units = airFree(dest->units);
      dest->units = airStrdup(src->units);
    }
  }

  return;
}

/*
******** nrrdAxisInfoCopy()
**
** For copying all the per-axis peripheral information.  Takes a
** permutation "map"; map[d] tells from which axis in input should the
** output axis d copy its information.  The length of this permutation
** array is nout->dim.  If map is NULL, the identity permutation is
** assumed.  If map[i]==-1 for any i in [0,dim-1], then nothing is
** copied into axis i of output.  The "bitflag" field controls which
** per-axis fields will NOT be copied; if bitflag==0, then all fields
** are copied.  The value of bitflag should be |'s of NRRD_AXIS_INFO_*
** defines.
**
** Decided to Not use Biff, since many times map will be NULL, in
** which case the only error is getting a NULL nrrd, or an invalid map
** permutation, which will probably be unlikely given the contexts in
** which this is called.  For the paranoid, the integer return value
** indicates error.
**
** Sun Feb 27 21:12:57 EST 2005: decided to allow nout==nin, so now
** use a local array of NrrdAxisInfo as buffer.
*/
int
nrrdAxisInfoCopy(Nrrd *nout, const Nrrd *nin, const int *axmap, int bitflag) {
  NrrdAxisInfo axisBuffer[NRRD_DIM_MAX];
  NrrdAxisInfo *axis;
  int d, from;
  
  if (!(nout && nin)) {
    return 1;
  }
  if (axmap) {
    for (d=0; d<nout->dim; d++) {
      if (-1 == axmap[d]) {
        continue;
      }
      if (!AIR_IN_CL(0, axmap[d], nin->dim-1)) {
        return 3;
      }
    }
  }
  if (nout == nin) {
    /* copy axis info to local buffer */
    for (d=0; d<nin->dim; d++) {
      _nrrdAxisInfoNewInit(axisBuffer + d);
      _nrrdAxisInfoCopy(axisBuffer + d, nin->axis + d, bitflag);
    }
    axis = axisBuffer;
  } else {
    axis = nin->axis;
  }
  for (d=0; d<nout->dim; d++) {
    from = axmap ? axmap[d] : d;
    if (-1 == from) {
      /* for this axis, we don't touch a thing */
      continue;
    }
    _nrrdAxisInfoCopy(nout->axis + d, axis + from, bitflag);
  }
  if (nout == nin) {
    /* free dynamically allocated stuff */
    for (d=0; d<nin->dim; d++) {
      _nrrdAxisInfoInit(axisBuffer + d);
    }
  }
  return 0;
}

/*
******** nrrdAxisInfoSet_nva()
**
** Simple means of setting fields of the axis array in the nrrd.
**
** type to pass for third argument:
**           nrrdAxisInfoSize: int*
**        nrrdAxisInfoSpacing: double*
**      nrrdAxisInfoThickness: double*
**            nrrdAxisInfoMin: double*
**            nrrdAxisInfoMax: double*
** nrrdAxisInfoSpaceDirection: double (*var)[NRRD_SPACE_DIM_MAX]
**         nrrdAxisInfoCenter: int*
**           nrrdAxisInfoKind: int*
**          nrrdAxisInfoLabel: char**
**          nrrdAxisInfoUnits: char**
*/
void
nrrdAxisInfoSet_nva(Nrrd *nrrd, int axInfo, const void *_info) {
  _nrrdAxisInfoSetPtrs info;
  int d, ii, exists, minii;
  
  if (!( nrrd 
         && AIR_IN_CL(1, nrrd->dim, NRRD_DIM_MAX) 
         && AIR_IN_OP(nrrdAxisInfoUnknown, axInfo, nrrdAxisInfoLast) 
         && _info )) {
    return;
  }
  info.P = _info;

  for (d=0; d<nrrd->dim; d++) {
    switch (axInfo) {
    case nrrdAxisInfoSize:
      nrrd->axis[d].size = info.I[d];
      break;
    case nrrdAxisInfoSpacing:
      nrrd->axis[d].spacing = info.D[d];
      break;
    case nrrdAxisInfoThickness:
      nrrd->axis[d].thickness = info.D[d];
      break;
    case nrrdAxisInfoMin:
      nrrd->axis[d].min = info.D[d];
      break;
    case nrrdAxisInfoMax:
      nrrd->axis[d].max = info.D[d];
      break;
    case nrrdAxisInfoSpaceDirection:
      /* we won't allow setting an invalid direction */
      exists = AIR_EXISTS(info.V[d][0]);
      minii = nrrd->spaceDim;
      for (ii=0; ii<nrrd->spaceDim; ii++) {
        nrrd->axis[d].spaceDirection[ii] = info.V[d][ii];
        if (exists ^ AIR_EXISTS(info.V[d][ii])) {
          minii = 0;
          break;
        }
      }
      for (ii=minii; ii<NRRD_SPACE_DIM_MAX; ii++) {
        nrrd->axis[d].spaceDirection[ii] = AIR_NAN;
      }
      break;
    case nrrdAxisInfoCenter:
      nrrd->axis[d].center = info.I[d];
      break;
    case nrrdAxisInfoKind:
      nrrd->axis[d].kind = info.I[d];
      break;
    case nrrdAxisInfoLabel:
      nrrd->axis[d].label = airFree(nrrd->axis[d].label);
      nrrd->axis[d].label = airStrdup(info.CP[d]);
      break;
    case nrrdAxisInfoUnits:
      nrrd->axis[d].units = airFree(nrrd->axis[d].units);
      nrrd->axis[d].units = airStrdup(info.CP[d]);
      break;
    }
  }
  if (nrrdAxisInfoSpaceDirection == axInfo) {
    for (d=nrrd->dim; d<NRRD_DIM_MAX; d++) {
      for (ii=0; ii<NRRD_SPACE_DIM_MAX; ii++) {
        nrrd->axis[d].spaceDirection[ii] = AIR_NAN;
      }
    }    
  }
  return;
}

/*
******** nrrdAxisInfoSet()
**
** var args front-end for nrrdAxisInfoSet_nva
**
** types to pass, one for each dimension:
**           nrrdAxisInfoSize: int
**        nrrdAxisInfoSpacing: double
**      nrrdAxisInfoThickness: double
**            nrrdAxisInfoMin: double
**            nrrdAxisInfoMax: double
** nrrdAxisInfoSpaceDirection: double*
**         nrrdAxisInfoCenter: int
**           nrrdAxisInfoKind: int
**          nrrdAxisInfoLabel: char*
**          nrrdAxisInfoUnits: char*
*/
void
nrrdAxisInfoSet(Nrrd *nrrd, int axInfo, ...) {
  NRRD_TYPE_BIGGEST *buffer[NRRD_DIM_MAX];
  _nrrdAxisInfoSetPtrs info;
  int d, ii;
  va_list ap;
  double *dp, svec[NRRD_DIM_MAX][NRRD_SPACE_DIM_MAX];

  if (!( nrrd 
         && AIR_IN_CL(1, nrrd->dim, NRRD_DIM_MAX) 
         && AIR_IN_OP(nrrdAxisInfoUnknown, axInfo, nrrdAxisInfoLast) )) {
    return;
  }

  info.P = buffer;
  va_start(ap, axInfo);
  for (d=0; d<nrrd->dim; d++) {
    switch (axInfo) {
    case nrrdAxisInfoSize:
      info.I[d] = va_arg(ap, int);
      /*
      printf("!%s: got int[%d] = %d\n", "nrrdAxisInfoSet", d, info.I[d]);
      */
      break;
    case nrrdAxisInfoSpaceDirection:
      dp = va_arg(ap, double*);  /* punting on using info enum */
      /*
      printf("!%s: got dp = %lu\n", "nrrdAxisInfoSet",
             (unsigned long)(dp));
      */
      for (ii=0; ii<nrrd->spaceDim; ii++) {
        /* nrrd->axis[d].spaceDirection[ii] = dp[ii]; */
        svec[d][ii] = dp[ii];
      }
      for (ii=nrrd->spaceDim; ii<NRRD_SPACE_DIM_MAX; ii++) {
        /* nrrd->axis[d].spaceDirection[ii] = AIR_NAN; */
        svec[d][ii] = dp[ii];
      }
      break;
    case nrrdAxisInfoCenter:
    case nrrdAxisInfoKind:
      info.I[d] = va_arg(ap, int);
      /*
      printf("!%s: got int[%d] = %d\n", 
             "nrrdAxisInfoSet", d, info.I[d]);
      */
      break;
    case nrrdAxisInfoSpacing:
    case nrrdAxisInfoThickness:
    case nrrdAxisInfoMin:
    case nrrdAxisInfoMax:
      info.D[d] = va_arg(ap, double);
      /*
      printf("!%s: got double[%d] = %g\n", 
             "nrrdAxisInfoSet", d, info.D[d]); 
      */
      break;
    case nrrdAxisInfoLabel:
      /* we DO NOT do the airStrdup() here because this pointer value is
         just going to be handed to nrrdAxisInfoSet_nva(), which WILL do the
         airStrdup(); we're not violating the rules for axis labels */
      info.CP[d] = va_arg(ap, char *);
      /*
      printf("!%s: got char*[%d] = |%s|\n", 
             "nrrdAxisInfoSet", d, info.CP[d]);
      */
      break;
    case nrrdAxisInfoUnits:
      /* see not above */
      info.CP[d] = va_arg(ap, char *);
      break;
    }
  }
  va_end(ap);

  if (nrrdAxisInfoSpaceDirection != axInfo) {
    /* now set the quantities which we've gotten from the var args */
    nrrdAxisInfoSet_nva(nrrd, axInfo, info.P);
  } else {
    nrrdAxisInfoSet_nva(nrrd, axInfo, svec);
  }
  
  return;
}

/*
******** nrrdAxisInfoGet_nva()
**
** get any of the axis fields into an array
**
** Note that getting axes labels involves implicitly allocating space
** for them, due to the action of airStrdup().  The user is
** responsible for free()ing these strings when done with them.
**
** type to pass for third argument:
**           nrrdAxisInfoSize: int*
**        nrrdAxisInfoSpacing: double*
**      nrrdAxisInfoThickness: double*
**            nrrdAxisInfoMin: double*
**            nrrdAxisInfoMax: double*
** nrrdAxisInfoSpaceDirection: double (*var)[NRRD_SPACE_DIM_MAX]
**         nrrdAxisInfoCenter: int*
**           nrrdAxisInfoKind: int*
**          nrrdAxisInfoLabel: char**
**          nrrdAxisInfoUnits: char**
*/
void
nrrdAxisInfoGet_nva(const Nrrd *nrrd, int axInfo, void *_info) {
  _nrrdAxisInfoGetPtrs info;
  int d, ii;
  
  if (!( nrrd 
         && AIR_IN_CL(1, nrrd->dim, NRRD_DIM_MAX) 
         && AIR_IN_OP(nrrdAxisInfoUnknown, axInfo, nrrdAxisInfoLast) )) {
    return;
  }
  
  info.P = _info;
  for (d=0; d<nrrd->dim; d++) {
    switch (axInfo) {
    case nrrdAxisInfoSize:
      info.I[d] = nrrd->axis[d].size;
      break;
    case nrrdAxisInfoSpacing:
      info.D[d] = nrrd->axis[d].spacing;
      break;
    case nrrdAxisInfoThickness:
      info.D[d] = nrrd->axis[d].thickness;
      break;
    case nrrdAxisInfoMin:
      info.D[d] = nrrd->axis[d].min;
      break;
    case nrrdAxisInfoMax:
      info.D[d] = nrrd->axis[d].max;
      break;
    case nrrdAxisInfoSpaceDirection:
      for (ii=0; ii<nrrd->spaceDim; ii++) {
        info.V[d][ii] = nrrd->axis[d].spaceDirection[ii];
      }
      for (ii=nrrd->spaceDim; ii<NRRD_SPACE_DIM_MAX; ii++) {
        info.V[d][ii] = AIR_NAN;
      }
      break;
    case nrrdAxisInfoCenter:
      info.I[d] = nrrd->axis[d].center;
      break;
    case nrrdAxisInfoKind:
      info.I[d] = nrrd->axis[d].kind;
      break;
    case nrrdAxisInfoLabel:
      /* note airStrdup()! */
      info.CP[d] = airStrdup(nrrd->axis[d].label);
      break;
    case nrrdAxisInfoUnits:
      /* note airStrdup()! */
      info.CP[d] = airStrdup(nrrd->axis[d].units);
      break;
    }
  }
  if (nrrdAxisInfoSpaceDirection == axInfo) {
    for (d=nrrd->dim; d<NRRD_DIM_MAX; d++) {
      for (ii=0; ii<NRRD_SPACE_DIM_MAX; ii++) {
        info.V[d][ii] = AIR_NAN;
      }
    }
  }
  return;
}

/*
** types to pass, one for each dimension:
**           nrrdAxisInfoSize: int*
**        nrrdAxisInfoSpacing: double*
**      nrrdAxisInfoThickness: double*
**            nrrdAxisInfoMin: double*
**            nrrdAxisInfoMax: double*
** nrrdAxisInfoSpaceDirection: double*
**         nrrdAxisInfoCenter: int*
**           nrrdAxisInfoKind: int*
**          nrrdAxisInfoLabel: char**
**          nrrdAxisInfoUnits: char**
*/
void
nrrdAxisInfoGet(const Nrrd *nrrd, int axInfo, ...) {
  void *buffer[NRRD_DIM_MAX], *ptr;
  _nrrdAxisInfoGetPtrs info;
  int d, ii;
  va_list ap;
  double svec[NRRD_DIM_MAX][NRRD_SPACE_DIM_MAX];

  if (!( nrrd 
         && AIR_IN_CL(1, nrrd->dim, NRRD_DIM_MAX) 
         && AIR_IN_OP(nrrdAxisInfoUnknown, axInfo, nrrdAxisInfoLast) )) {
    return;
  }

  if (nrrdAxisInfoSpaceDirection != axInfo) {
    info.P = buffer;
    nrrdAxisInfoGet_nva(nrrd, axInfo, info.P);
  } else {
    nrrdAxisInfoGet_nva(nrrd, axInfo, svec);
  }

  va_start(ap, axInfo);
  for (d=0; d<nrrd->dim; d++) {
    ptr = va_arg(ap, void*);
    /*
    printf("!%s(%d): ptr = %lu\n", 
           "nrrdAxisInfoGet", d, (unsigned long)ptr);
    */
    switch (axInfo) {
    case nrrdAxisInfoSize:
      *((int*)ptr) = info.I[d];
      /* printf("!%s: got int[%d] = %d\n", 
         "nrrdAxisInfoGet", d, *((int*)ptr)); */
      break;
    case nrrdAxisInfoSpacing:
    case nrrdAxisInfoThickness:
    case nrrdAxisInfoMin:
    case nrrdAxisInfoMax:
      *((double*)ptr) = info.D[d];
      /* printf("!%s: got double[%d] = %lg\n", "nrrdAxisInfoGet", d,
       *((double*)ptr)); */
      break;
    case nrrdAxisInfoSpaceDirection:
      for (ii=0; ii<nrrd->spaceDim; ii++) {
        ((double*)ptr)[ii] = svec[d][ii];
      }
      for (ii=nrrd->spaceDim; ii<NRRD_SPACE_DIM_MAX; ii++) {
        ((double*)ptr)[ii] = AIR_NAN;
      }
      break;
    case nrrdAxisInfoCenter:
    case nrrdAxisInfoKind:
      *((int*)ptr) = info.I[d];
      /* printf("!%s: got int[%d] = %d\n", 
         "nrrdAxisInfoGet", d, *((int*)ptr)); */
      break;
    case nrrdAxisInfoLabel:
    case nrrdAxisInfoUnits:
      /* we DO NOT do the airStrdup() here because this pointer value just
         came from nrrdAxisInfoGet_nva(), which already did the airStrdup() */
      *((char**)ptr) = info.CP[d];
      /* printf("!%s: got char*[%d] = |%s|\n", "nrrdAxisInfoSet", d, 
       *((char**)ptr)); */
      break;
    }
  }
  va_end(ap);

  return;
}

/*
** _nrrdCenter()
**
** for nrrdCenterCell and nrrdCenterNode, return will be the same
** as input.  Converts nrrdCenterUnknown into nrrdDefCenter,
** and then clamps to (nrrdCenterUnknown+1, nrrdCenterLast-1).
**
** Thus, this ALWAYS returns nrrdCenterNode or nrrdCenterCell
** (as long as those are the only two centering schemes).
*/
int
_nrrdCenter(int center) {
  
  center =  (nrrdCenterUnknown == center
             ? nrrdDefCenter
             : center);
  center = AIR_CLAMP(nrrdCenterUnknown+1, center, nrrdCenterLast-1);
  return center;
}

int
_nrrdCenter2(int center, int defCenter) {
  
  center =  (nrrdCenterUnknown == center
             ? defCenter
             : center);
  center = AIR_CLAMP(nrrdCenterUnknown+1, center, nrrdCenterLast-1);
  return center;
}


/*
******** nrrdAxisInfoPos()
** 
** given a nrrd, an axis, and a (floating point) index space position,
** return the position implied the axis's min, max, and center
** Does the opposite of nrrdAxisIdx().
**
** does not use biff
*/
double
nrrdAxisInfoPos(const Nrrd *nrrd, int ax, double idx) {
  int center, size;
  double min, max;
  
  if (!( nrrd && AIR_IN_CL(0, ax, nrrd->dim-1) )) {
    return AIR_NAN;
  }
  center = _nrrdCenter(nrrd->axis[ax].center);
  min = nrrd->axis[ax].min;
  max = nrrd->axis[ax].max;
  size = nrrd->axis[ax].size;
  
  return NRRD_POS(center, min, max, size, idx);
}

/*
******** nrrdAxisInfoIdx()
** 
** given a nrrd, an axis, and a (floating point) world space position,
** return the index implied the axis's min, max, and center.
** Does the opposite of nrrdAxisPos().
**
** does not use biff
*/
double
nrrdAxisInfoIdx(const Nrrd *nrrd, int ax, double pos) {
  int center, size;
  double min, max;
  
  if (!( nrrd && AIR_IN_CL(0, ax, nrrd->dim-1) )) {
    return AIR_NAN;
  }
  center = _nrrdCenter(nrrd->axis[ax].center);
  min = nrrd->axis[ax].min;
  max = nrrd->axis[ax].max;
  size = nrrd->axis[ax].size;

  return NRRD_IDX(center, min, max, size, pos);
}

/*
******** nrrdAxisInfoPosRange()
**
** given a nrrd, an axis, and two (floating point) index space positions,
** return the range of positions implied the axis's min, max, and center
** The opposite of nrrdAxisIdxRange()
*/
void
nrrdAxisInfoPosRange(double *loP, double *hiP,
                     const Nrrd *nrrd, int ax, 
                     double loIdx, double hiIdx) {
  int center, size, flip = 0;
  double min, max, tmp;

  if (!( loP && hiP && nrrd && AIR_IN_CL(0, ax, nrrd->dim-1) )) {
    *loP = *hiP = AIR_NAN;
    return;
  }
  center = _nrrdCenter(nrrd->axis[ax].center);
  min = nrrd->axis[ax].min;
  max = nrrd->axis[ax].max;
  size = nrrd->axis[ax].size;

  if (loIdx > hiIdx) {
    flip = 1;
    tmp = loIdx; loIdx = hiIdx; hiIdx = tmp;
  }
  if (nrrdCenterCell == center) {
    *loP = AIR_AFFINE(0, loIdx, size, min, max);
    *hiP = AIR_AFFINE(0, hiIdx+1, size, min, max);
  } else {
    *loP = AIR_AFFINE(0, loIdx, size-1, min, max);
    *hiP = AIR_AFFINE(0, hiIdx, size-1, min, max);
  }
  if (flip) {
    tmp = *loP; *loP = *hiP; *hiP = tmp;
  }

  return;
}

/*
******** nrrdAxisInfoIdxRange()
**
** given a nrrd, an axis, and two (floating point) world space positions,
** return the range of index space implied the axis's min, max, and center
** The opposite of nrrdAxisPosRange().
**
** Actually- there are situations where sending an interval through
** nrrdAxisIdxRange -> nrrdAxisPosRange -> nrrdAxisIdxRange
** such as in cell centering, when the range of positions given does
** not even span one sample.  Such as:
** axis->size = 4, axis->min = -4, axis->max = 4, loPos = 0, hiPos = 1
** --> nrrdAxisIdxRange == (2, 1.5) --> nrrdAxisPosRange == (2, -1)
** The basic problem is that because of the 0.5 offset inherent in
** cell centering, there are situations where (in terms of the arguments
** to nrrdAxisIdxRange()) loPos < hiPos, but *loP > *hiP.
*/
void
nrrdAxisInfoIdxRange(double *loP, double *hiP,
                     const Nrrd *nrrd, int ax, 
                     double loPos, double hiPos) {
  int center, size, flip = 0;
  double min, max, tmp;

  if (!( loP && hiP && nrrd && AIR_IN_CL(0, ax, nrrd->dim-1) )) {
    *loP = *hiP = AIR_NAN;
    return;
  }
  center = _nrrdCenter(nrrd->axis[ax].center);
  min = nrrd->axis[ax].min;
  max = nrrd->axis[ax].max;
  size = nrrd->axis[ax].size;

  if (loPos > hiPos) {
    flip = 1;
    tmp = loPos; loPos = hiPos; hiPos = tmp;
  }
  if (nrrdCenterCell == center) {
    if (min < max) {
      *loP = AIR_AFFINE(min, loPos, max, 0, size);
      *hiP = AIR_AFFINE(min, hiPos, max, -1, size-1);
    } else {
      *loP = AIR_AFFINE(min, loPos, max, -1, size-1);
      *hiP = AIR_AFFINE(min, hiPos, max, 0, size);
    }
  } else {
    *loP = AIR_AFFINE(min, loPos, max, 0, size-1);
    *hiP = AIR_AFFINE(min, hiPos, max, 0, size-1);
  }
  if (flip) {
    tmp = *loP; *loP = *hiP; *hiP = tmp;
  }

  return;
}

void
nrrdAxisInfoSpacingSet(Nrrd *nrrd, int ax) {
  int sign;
  double min, max, tmp;

  if (!( nrrd && AIR_IN_CL(0, ax, nrrd->dim-1) ))
    return;
  
  min = nrrd->axis[ax].min;
  max = nrrd->axis[ax].max;
  if (!( AIR_EXISTS(min) && AIR_EXISTS(max) )) {
    /* there's no actual basis on which to set the spacing information,
       but we have to set it something, so here goes ... */
    nrrd->axis[ax].spacing = nrrdDefSpacing;
    return;
  }

  if (min > max) {
    tmp = min; min = max; max = tmp;
    sign = -1;
  } else {
    sign = 1;
  }

  /* the skinny */
  nrrd->axis[ax].spacing = NRRD_SPACING(_nrrdCenter(nrrd->axis[ax].center),
                                        min, max, nrrd->axis[ax].size);
  nrrd->axis[ax].spacing *= sign;

  return;
}

void
nrrdAxisInfoMinMaxSet(Nrrd *nrrd, int ax, int defCenter) {
  int center;
  double spacing;

  if (!( nrrd && AIR_IN_CL(0, ax, nrrd->dim-1) ))
    return;
  
  center = _nrrdCenter2(nrrd->axis[ax].center, defCenter);
  spacing = nrrd->axis[ax].spacing;
  if (!AIR_EXISTS(spacing))
    spacing = nrrdDefSpacing;
  if (nrrdCenterCell == center) {
    nrrd->axis[ax].min = 0;
    nrrd->axis[ax].max = spacing*nrrd->axis[ax].size;
  } else {
    nrrd->axis[ax].min = 0;
    nrrd->axis[ax].max = spacing*(nrrd->axis[ax].size - 1);
  }
  
  return;
}

/*
******** nrrdSpacingCalculate
**
** Determine nrrdSpacingStatus, and whatever can be calculated about
** spacing for a given axis.  Takes a nrrd, an axis, a double pointer
** (for returning a scalar), a space vector, and an int pointer for
** returning the known length of the space vector.
**
** The behavior of what has been set by the function is determined by
** the return value, which takes values from the nrrdSpacingStatus*
** enum, as follows:
**
** returned status value:            what it means, and what it set
** ---------------------------------------------------------------------------
** nrrdSpacingStatusUnknown          Something about the given arguments is
**                                   invalid.
**                                   *spacing = NaN,
**                                   vector = all NaNs
**
** nrrdSpacingStatusNone             There is no spacing info at all:
**                                   *spacing = NaN,
**                                   vector = all NaNs
**
** nrrdSpacingStatusScalarNoSpace    There is no surrounding space, but the
**                                   axis's spacing was known.
**                                   *spacing = axis->spacing,
**                                   vector = all NaNs
**
** nrrdSpacingStatusScalarWithSpace  There *is* a surrounding space, but the
**                                   given axis does not live in that space,
**                                   because it has no space direction.  Caller
**                                   may want to think about what's going on.
**                                   *spacing = axis->spacing,
**                                   vector = all NaNs
**
** nrrdSpacingStatusDirection        There is a surrounding space, in which
**                                   this axis has a direction V:
**                                   *spacing = |V| (length of direction),
**                                   vector = V/|V| (normalized direction)
*/
int
nrrdSpacingCalculate(const Nrrd *nrrd, int ax,
                     double *spacing, double vector[NRRD_SPACE_DIM_MAX]) {
  int ret;
  
  if (!( nrrd && spacing && vector
         && AIR_IN_CL(0, ax, nrrd->dim-1)
         && !_nrrdCheck(nrrd, AIR_FALSE, AIR_FALSE) )) {
    /* there's a problem with the arguments.  Note: the _nrrdCheck()
       call does not check on non-NULL-ity of nrrd->data */
    ret = nrrdSpacingStatusUnknown;
    if (spacing) { 
      *spacing = AIR_NAN;
    }
    if (vector) {
      _nrrdSpaceVecSetNaN(vector);
    }
  } else {
    if (AIR_EXISTS(nrrd->axis[ax].spacing)) {
      if (nrrd->spaceDim > 0) {
        ret = nrrdSpacingStatusScalarWithSpace;
      } else {
        ret = nrrdSpacingStatusScalarNoSpace;
      }
      *spacing = nrrd->axis[ax].spacing;
      _nrrdSpaceVecSetNaN(vector);      
    } else {
      if (nrrd->spaceDim > 0) {
        ret = nrrdSpacingStatusDirection;
        *spacing = _nrrdSpaceVecNorm(nrrd->spaceDim, 
                                     nrrd->axis[ax].spaceDirection);
        _nrrdSpaceVecScale(vector, 1.0/(*spacing),
                           nrrd->axis[ax].spaceDirection);
      } else {
        ret = nrrdSpacingStatusNone;
        *spacing = AIR_NAN;
        _nrrdSpaceVecSetNaN(vector);
      }
    }
  }
  return ret;
}
