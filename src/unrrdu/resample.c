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

#include "private.h"

char *resampleName = "resample";
#define INFO "Filtering and {up,down}sampling with a seperable kernel"
char *resampleInfo = INFO;
char *resampleInfoL = (INFO
		       ". Provides simplified access to nrrdSpatialResample() "
		       "by assuming (among other things) that the same kernel "
		       "is used for resampling "
		       "every axis (every axis which is being resampled), and "
		       "by assuming that the whole axis is being resampled "
		       "(no cropping or padding).  Chances are, you should "
		       "use defaults for \"-b\" and \"-v\" and worry only "
		       "about the \"-s\" and \"-k\" options.  This resampling "
		       "respects the difference between cell- and "
		       "node-centered data.");

/*
** unuNrrdKernel
** 
** this is what will be parsed from the command-line: a kernel and its
** parameter list
*/
typedef struct {
  NrrdKernel *kernel;
  double param[NRRD_KERNEL_PARAMS_MAX];
} unuNrrdKernel;

int
unuParseKernel(void *ptr, char *str, char err[AIR_STRLEN_HUGE]) {
  unuNrrdKernel *ker;
  char me[]="unuParseKernel", *nerr;

  if (!(ptr && str)) {
    sprintf(err, "%s: got NULL pointer", me);
    return 1;
  }
  ker = ptr;
  if (nrrdKernelParse(&(ker->kernel), ker->param, str)) {
    nerr = biffGetDone(NRRD);
    strncpy(err, nerr, AIR_STRLEN_HUGE-1);
    free(nerr);
    return 1;
  }
  return 0;
}

hestCB unuKernelHestCB = {
  sizeof(unuNrrdKernel),
  "kernel specification",
  unuParseKernel,
  NULL
};

/*
** unuParseScale
**
** parse "=", "x<float>", and "<int>".  These possibilities are represented
** for axis i by setting scale[0 + 2*i] to 0, 1, or 2, respectively.
*/
int
unuParseScale(void *ptr, char *str, char err[AIR_STRLEN_HUGE]) {
  char me[]="unuParseScale";
  float *scale;
  int num;
  
  if (!(ptr && str)) {
    sprintf(err, "%s: got NULL pointer", me);
    return 1;
  }
  scale = ptr;
  if (!strcmp("=", str)) {
    scale[0] = 0.0;
    scale[1] = 0.0;
    return 0;
  }

  /* else */
  if ('x' == str[0]) {
    if (1 != sscanf(str+1, "%f", scale+1)) {
      sprintf(err, "%s: can't parse \"%s\" as x<float>", me, str);
      return 1;
    }
    scale[0] = 1.0;
  }
  else {
    if (1 != sscanf(str, "%d", &num)) {
      sprintf(err, "%s: can't parse \"%s\" as int", me, str);
      return 1;
    }
    scale[0] = 2.0;
    scale[1] = num;
  }
  return 0;
}

hestCB unuScaleHestCB = {
  2*sizeof(float),
  "sampling specification",
  unuParseScale,
  NULL
};

int
resampleMain(int argc, char **argv, char *me) {
  hestOpt *opt = NULL;
  char *out, *err;
  Nrrd *nin, *nout;
  int d, scaleLen, bb;
  airArray *mop;
  float *scale;
  double padVal;
  NrrdResampleInfo *info;
  unuNrrdKernel unuk;

  OPT_ADD_NIN(nin, "input nrrd");
  hestOptAdd(&opt, "s", "s0 ", airTypeOther, 1, -1, &scale, NULL,
	     "For each axis, information about how many samples in output:\n "
	     "\b\bo \"=\": leave this axis untouched: no resampling "
	     "whatsoever\n "
	     "\b\bo \"x<float>\": number of output samples is some scaling of "
	     " the number samples in input, multiplied by <float>\n "
	     "\b\bo \"<int>\": exact number of samples",
	     &scaleLen, NULL, &unuScaleHestCB);
  hestOptAdd(&opt, "k", "kern", airTypeOther, 1, 1, &unuk,
	     "quartic:0.0834",
	     "The kernel to use for resampling.  Possibilities include:\n "
	     "\b\bo \"box\": nearest neighbor interpolation\n "
	     "\b\bo \"tent\": linear interpolation\n "
	     "\b\bo \"cubic:B,C\": Mitchell/Netravali BC-family of "
	     "cubics:\n "
	     "\t\t\"cubic:1,0\": B-spline; maximal blurring\n "
	     "\t\t\"cubic:0,0.5\": Catmull-Rom; good interpolating kernel\n "
	     "\b\bo \"quartic:A\": 1-parameter family of "
	     "interpolating quartics (\"quartic:0.0834\" is most accurate)\n "
	     "\b\bo \"gauss:S,C\": Gaussian blurring, with standard deviation "
	     "S and cut-off at C standard deviations",
	     NULL, NULL, &unuKernelHestCB);
  hestOptAdd(&opt, "b", "behavior", airTypeEnum, 1, 1, &bb, "bleed",
	     "How to handle samples beyond the input bounds:\n "
	     "\b\bo \"pad\": use some specified value\n "
	     "\b\bo \"bleed\": extend border values outward\n "
	     "\b\bo \"wrap\": wrap-around to other side", 
	     NULL, &nrrdBoundary);
  hestOptAdd(&opt, "v", "value", airTypeDouble, 1, 1, &padVal, "0.0",
	     "for \"pad\" boundary behavior, pad with this value");
  OPT_ADD_NOUT(out, "output nrrd");

  mop = airMopInit();
  airMopAdd(mop, opt, (airMopper)hestOptFree, airMopAlways);
  info = nrrdResampleInfoNew();
  airMopAdd(mop, info, (airMopper)nrrdResampleInfoNix, airMopAlways);
  
  USAGE(resampleInfoL);
  PARSE();
  airMopAdd(mop, opt, (airMopper)hestParseFree, airMopAlways);

  if (scaleLen != nin->dim) {
    fprintf(stderr, "%s: # sampling sizes (%d) != input nrrd dimension (%d)\n",
	    me, scaleLen, nin->dim);
    return 1;
  }
  for (d=0; d<=nin->dim-1; d++) {
    /* this may be over-written below */
    info->kernel[d] = unuk.kernel;
    switch((int)scale[0 + 2*d]) {
    case 0:
      /* no resampling */
      info->kernel[d] = NULL;
      break;
    case 1:
      /* scaling of input # samples */
      info->samples[d] = scale[1 + 2*d]*nin->axis[d].size;
      break;
    case 2:
      /* example # of samples */
      info->samples[d] = scale[1 + 2*d];
      break;
    }
    memcpy(info->param[d], unuk.param, NRRD_KERNEL_PARAMS_MAX*sizeof(double));
    if (info->kernel[d] &&
	(!( AIR_EXISTS(nin->axis[d].min) && AIR_EXISTS(nin->axis[d].max))) ) {
      nrrdAxisMinMaxSet(nin, d);
    }
    info->min[d] = nin->axis[d].min;
    info->max[d] = nin->axis[d].max;
  }
  info->boundary = bb;
  info->type = nin->type;
  info->padValue = padVal;

  nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
  
  if (nrrdSpatialResample(nout, nin, info)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error resampling nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  SAVE(nout, NULL);

  airMopOkay(mop);
  return 0;
}
