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

#include "moss.h"
#include "privateMoss.h"

int 
mossSamplerImageSet (mossSampler *smplr, Nrrd *image, float *bg) {
  char me[]="mossSamplerImageSet", err[AIR_STRLEN_MED];
  int ci, ncol;
  
  if (!(smplr && image)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(MOSS, err); return 1;
  }
  if (mossImageCheck(image)) {
    sprintf(err, "%s: ", me);
    biffAdd(MOSS, err); return 1;
  }
  smplr->image = image;
  smplr->flag[mossFlagImage] = AIR_TRUE;
  ncol = MOSS_NCOL(image);
  AIR_FREE(smplr->bg);
  smplr->bg = (float*)calloc(ncol, sizeof(float));
  if (bg) {
    for (ci=0; ci<ncol; ci++) {
      smplr->bg[ci] = bg[ci];
    }
  }
  return 0;
}

int
mossSamplerKernelSet (mossSampler *smplr, NrrdKernel *kernel, double *kparm) {
  char me[]="mossSamplerKernelSet", err[AIR_STRLEN_MED];
  int i;

  if (!(smplr && kernel && kparm)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(MOSS, err); return 1;
  }
  smplr->kernel = kernel;
  for (i=0; i<kernel->numParm; i++) {
    smplr->kparm[i] = kparm[i];
  }
  smplr->flag[mossFlagKernel] = AIR_TRUE;
  return 0;
}

int
mossSamplerUpdate (mossSampler *smplr) {
  char me[]="mossSamplerUpdate", err[AIR_STRLEN_MED];
  int ncol=0, fdiam=0;

  if (!(smplr)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(MOSS, err); return 1;
  }
  
  if (smplr->flag[mossFlagImage]) {
    ncol = MOSS_NCOL(smplr->image);
    if (ncol != smplr->ncol) {
      mossSamplerEmpty(smplr);
      smplr->ncol = ncol;
    }
  }
  if (smplr->flag[mossFlagKernel]) {
    fdiam = 2*AIR_ROUNDUP(smplr->kernel->support(smplr->kparm));
    if (fdiam != smplr->fdiam) {
      mossSamplerEmpty(smplr);
      smplr->fdiam = fdiam;
    }
  }
  if (!(smplr->ivc)) {
    if (mossSamplerFill(smplr, fdiam, ncol)) {
      sprintf(err, "%s: ", me);
      biffAdd(MOSS, err); return 1;
    }
  }

  return 0;
}

int
mossSamplerSample (float *val, mossSampler *smplr, double xPos, double yPos) {
  char me[]="mossSamplerSample", err[AIR_STRLEN_MED];
  int i, xi, yi, ci, sx, sy, fdiam, frad, ncol;
  double xf, yf, tmp;
  float (*lup)(void *v, size_t I);
  
  if (!(val && smplr)) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(MOSS, err); return 1;
  }
  if (!(smplr->ivc)) {
    sprintf(err, "%s: given sampler not ready (no caches)", me);
    biffAdd(MOSS, err); return 1;
  }

  /* set {x,y}Idx, set {x,y}Fslw to sample locations */
  if (mossVerbose) {
    fprintf(stderr, "%s: pos = %g %g\n", me, xPos, yPos);
  }
  sx = MOSS_SX(smplr->image);
  sy = MOSS_SY(smplr->image);
  xi = floor(xPos); xf = xPos - xi;
  yi = floor(yPos); yf = yPos - yi;
  fdiam = smplr->fdiam;
  frad = fdiam/2;
  for (i=0; i<fdiam; i++) {
    smplr->xIdx[i] = xi + i - frad + 1;
    smplr->yIdx[i] = yi + i - frad + 1;
    smplr->xFslw[i] = xf - i + frad - 1;
    smplr->yFslw[i] = yf - i + frad - 1;
  }
  if (mossVerbose) {
    fprintf(stderr, " --> xIdx: %d %d ; xFsl %g %g\n",
	    smplr->xIdx[0], smplr->xIdx[1],
	    smplr->xFslw[0], smplr->xFslw[1]);
    fprintf(stderr, "     yIdx: %d %d ; yFsl %g %g\n",
	    smplr->yIdx[0], smplr->yIdx[1],
	    smplr->yFslw[0], smplr->yFslw[1]);
  }
  switch(smplr->boundary) {
  case nrrdBoundaryBleed:
    for (i=0; i<fdiam; i++) {
      smplr->xIdx[i] = AIR_CLAMP(0, smplr->xIdx[i], sx-1);
      smplr->yIdx[i] = AIR_CLAMP(0, smplr->yIdx[i], sy-1);
    }
    break;
  case nrrdBoundaryWrap:
    for (i=0; i<fdiam; i++) {
      smplr->xIdx[i] = AIR_MOD(smplr->xIdx[i], sx);
      smplr->yIdx[i] = AIR_MOD(smplr->yIdx[i], sy);
    }
    break;
  case nrrdBoundaryPad:
    /* this is handled later */
    break;
  default:
    sprintf(err, "%s: sorry, %s boundary not implemented", me,
	    airEnumStr(nrrdBoundary, smplr->boundary));
    biffAdd(MOSS, err); return 1;
  }
  if (mossVerbose) {
    fprintf(stderr, " --> xIdx: %d %d ; xFsl %g %g\n",
	    smplr->xIdx[0], smplr->xIdx[1],
	    smplr->xFslw[0], smplr->xFslw[1]);
  }

  /* copy values to ivc, set {x,y}Fslw to filter sample weights */
  lup = nrrdFLookup[smplr->image->type];
  ncol = smplr->ncol;
  if (nrrdBoundaryPad == smplr->boundary) {
    for (yi=0; yi<fdiam; yi++) {
      for (xi=0; xi<fdiam; xi++) {
	if (AIR_IN_CL(0, smplr->xIdx[xi], sx-1)
	    && AIR_IN_CL(0, smplr->yIdx[yi], sy-1)) {
	  for (ci=0; ci<ncol; ci++) {
	    smplr->ivc[xi + fdiam*(yi + fdiam*ci)] =
	      lup(smplr->image->data,
		  ci + ncol*(smplr->xIdx[xi] + sx*smplr->yIdx[yi]));
	  }
	} else {
	  for (ci=0; ci<ncol; ci++) {
	    smplr->ivc[xi + fdiam*(yi + fdiam*ci)] = smplr->bg[ci];
	  }
	}
      }
    }
  } else {
    for (yi=0; yi<fdiam; yi++) {
      for (xi=0; xi<fdiam; xi++) {
	for (ci=0; ci<ncol; ci++) {
	  smplr->ivc[xi + fdiam*(yi + fdiam*ci)] =
	    lup(smplr->image->data,
		ci + ncol*(smplr->xIdx[xi] + sx*smplr->yIdx[yi]));
	}
      }
    }
  }
  smplr->kernel->evalN_d(smplr->xFslw, smplr->xFslw, fdiam, smplr->kparm);
  smplr->kernel->evalN_d(smplr->yFslw, smplr->yFslw, fdiam, smplr->kparm);

  /* do convolution */
  memset(val, 0, ncol*sizeof(float));
  for (ci=0; ci<ncol; ci++) {
    for (yi=0; yi<fdiam; yi++) {
      tmp = 0;
      for (xi=0; xi<fdiam; xi++) {
	tmp += smplr->xFslw[xi]*smplr->ivc[xi + fdiam*(yi + fdiam*ci)];
      }
      val[ci] += smplr->yFslw[yi]*tmp;
    }
  }  
  
  return 0;
}
