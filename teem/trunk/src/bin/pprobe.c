/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2006, 2005  Gordon Kindlmann
  Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998  University of Utah

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License
  (LGPL) as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  The terms of redistributing and/or modifying this software also
  include exceptions to the LGPL that facilitate static linking.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <stdio.h>

#include <teem/biff.h>
#include <teem/hest.h>
#include <teem/nrrd.h>
#include <teem/gage.h>
#include <teem/ten.h>
#include <teem/limn.h>

#define SPACING(spc) (AIR_EXISTS(spc) ? spc: nrrdDefaultSpacing)

/* copied this from ten.h; I don't want gage to depend on ten */
#define PROBE_MAT2LIST(l, m) ( \
   (l)[1] = (m)[0],          \
   (l)[2] = (m)[3],          \
   (l)[3] = (m)[6],          \
   (l)[4] = (m)[4],          \
   (l)[5] = (m)[7],          \
   (l)[6] = (m)[8] )

int
probeParseKind(void *ptr, char *str, char err[AIR_STRLEN_HUGE]) {
  char me[] = "probeParseKind";
  gageKind **kindP;
  
  if (!(ptr && str)) {
    sprintf(err, "%s: got NULL pointer", me);
    return 1;
  }
  kindP = (gageKind **)ptr;
  airToLower(str);
  if (!strcmp(gageKindScl->name, str)) {
    *kindP = gageKindScl;
  } else if (!strcmp(gageKindVec->name, str)) {
    *kindP = gageKindVec;
  } else if (!strcmp(tenGageKind->name, str)) {
    *kindP = tenGageKind;
  } else if (!strcmp(TEN_DWI_GAGE_KIND_NAME, str)) {
    *kindP = tenDwiGageKindNew();
  } else {
    sprintf(err, "%s: not \"%s\", \"%s\", \"%s\", or \"%s\"", me,
            gageKindScl->name, gageKindVec->name,
            tenGageKind->name, TEN_DWI_GAGE_KIND_NAME);
    return 1;
  }

  return 0;
}

void *
probeParseKindDestroy(void *ptr) {
  gageKind *kind;
  
  if (ptr) {
    kind = AIR_CAST(gageKind *, ptr);
    if (!strcmp(TEN_DWI_GAGE_KIND_NAME, kind->name)) {
      tenDwiGageKindNix(kind);
    }
  }
  return NULL;
}

hestCB probeKindHestCB = {
  sizeof(gageKind *),
  "kind",
  probeParseKind,
  probeParseKindDestroy
}; 

void
printans(FILE *file, const double *ans, int len) {
  int a;

  AIR_UNUSED(file);
  for (a=0; a<=len-1; a++) {
    if (a) {
      printf(", ");
    }
    printf("%g", ans[a]);
  }
}

char *probeInfo = ("Uses gageProbe() to query scalar or vector volumes "
                   "at a single probe location.");

int
main(int argc, char *argv[]) {
  gageKind *kind;
  char *me, *whatS, *err, *outS;
  hestParm *hparm;
  hestOpt *hopt = NULL;
  NrrdKernelSpec *k00, *k11, *k22, *kSS, *kSSblur;
  float pos[3], lineInfo[4];
  double gmc, rangeSS[2], idxSS;
  unsigned int ansLen, numSS, ninSSIdx, lineStepNum;
  int what, E=0, renorm, SSrenorm, verbose;
  const double *answer;
  Nrrd *nin, **ninSS=NULL, *nout=NULL;
  gageContext *ctx;
  gagePerVolume *pvl;
  limnPolyData *lpld=NULL;
  airArray *mop;

  Nrrd *ngrad=NULL, *nbmat=NULL;
  double bval;
  unsigned int *skip, skipNum;

  mop = airMopNew();
  me = argv[0];
  hparm = hestParmNew();
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);
  hparm->elideSingleOtherType = AIR_TRUE;
  hestOptAdd(&hopt, "i", "nin", airTypeOther, 1, 1, &nin, NULL,
             "input volume", NULL, NULL, nrrdHestNrrd);
  hestOptAdd(&hopt, "k", "kind", airTypeOther, 1, 1, &kind, NULL,
             "\"kind\" of volume (\"scalar\", \"vector\", or \"tensor\")",
             NULL, NULL, &probeKindHestCB);
  hestOptAdd(&hopt, "p", "x y z", airTypeFloat, 3, 3, pos, NULL,
             "the position in index space at which to probe");
  hestOptAdd(&hopt, "pi", "lpld in", airTypeOther, 1, 1, &lpld, "",
             "input polydata (overrides \"-p\")",
             NULL, NULL, limnHestPolyDataLMPD);
  hestOptAdd(&hopt, "pl", "x y z s", airTypeFloat, 4, 4, lineInfo, "0 0 0 0",
             "probe along line, instead of at point.  "
             "The \"-p\" three coords are the line start point. "
             "If \"s\" is zero, (x,y,z) is the line end point. "
             "If \"s\" is non-zero, (x,y,z) is the line direction, "
             "which is scaled to have length \"s\", "
             "and then used as the step between line samples. ");
  hestOptAdd(&hopt, "pln", "num", airTypeUInt, 1, 1, &lineStepNum, "0",
             "if non-zero, number of steps of probing to do along line, "
             "which overrides \"-p\" and \"-pi\"");
  hestOptAdd(&hopt, "v", "verbosity", airTypeInt, 1, 1, &verbose, "1", 
             "verbosity level");
  hestOptAdd(&hopt, "q", "query", airTypeString, 1, 1, &whatS, NULL,
             "the quantity (scalar, vector, or matrix) to learn by probing");
  hestOptAdd(&hopt, "k00", "kern00", airTypeOther, 1, 1, &k00,
             "tent", "kernel for gageKernel00",
             NULL, NULL, nrrdHestKernelSpec);
  hestOptAdd(&hopt, "k11", "kern11", airTypeOther, 1, 1, &k11,
             "cubicd:1,0", "kernel for gageKernel11",
             NULL, NULL, nrrdHestKernelSpec);
  hestOptAdd(&hopt, "k22", "kern22", airTypeOther, 1, 1, &k22,
             "cubicdd:1,0", "kernel for gageKernel22",
             NULL, NULL, nrrdHestKernelSpec);

  hestOptAdd(&hopt, "ssn", "SS #", airTypeUInt, 1, 1, &numSS,
             "0", "how many scale-space samples to evaluate, or, "
             "0 to turn-off all scale-space behavior");
  hestOptAdd(&hopt, "ssr", "scale range", airTypeDouble, 2, 2, rangeSS,
             "nan nan", "range of scales in scale-space");
  hestOptAdd(&hopt, "ssi", "SS idx", airTypeDouble, 1, 1, &idxSS, "0",
             "position at which to sample in scale-space");
  hestOptAdd(&hopt, "kssblur", "kernel", airTypeOther, 1, 1, &kSSblur,
             "gauss:1,5", "blurring kernel, to sample scale space",
             NULL, NULL, nrrdHestKernelSpec);
  hestOptAdd(&hopt, "kss", "kernel", airTypeOther, 1, 1, &kSS,
             "tent", "kernel for reconstructing from scale space samples",
             NULL, NULL, nrrdHestKernelSpec);
  hestOptAdd(&hopt, "ssrn", "ssrn", airTypeInt, 1, 1, &SSrenorm, "0",
             "enable derivative normalization based on scale space");

  hestOptAdd(&hopt, "rn", NULL, airTypeInt, 0, 0, &renorm, NULL,
             "renormalize kernel weights at each new sample location. "
             "\"Accurate\" kernels don't need this; doing it always "
             "makes things go slower");
  hestOptAdd(&hopt, "gmc", "min gradmag", airTypeDouble, 1, 1, &gmc,
             "0.0", "For curvature-based queries, use zero when gradient "
             "magnitude is below this");
  hestOptAdd(&hopt, "o", "nout", airTypeString, 1, 1, &outS, "-",
             "output array, when probing on polydata vertices");
  hestParseOrDie(hopt, argc-1, argv+1, hparm,
                 me, probeInfo, AIR_TRUE, AIR_TRUE, AIR_TRUE);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  what = airEnumVal(kind->enm, whatS);
  if (-1 == what) {
    /* -1 indeed always means "unknown" for any gageKind */
    fprintf(stderr, "%s: couldn't parse \"%s\" as measure of \"%s\" volume\n",
            me, whatS, kind->name);
    hestUsage(stderr, hopt, me, hparm);
    hestGlossary(stderr, hopt, hparm);
    airMopError(mop);
    return 1;
  }
  
  if (ELL_4V_LEN(lineInfo) && !lineStepNum) {
    fprintf(stderr, "%s: gave line info (\"-pl\") but not # samples (\"-pln\")",
            me);
    hestUsage(stderr, hopt, me, hparm);
    hestGlossary(stderr, hopt, hparm);
    airMopError(mop);
    return 1;
  }

  /* special set-up required for DWI kind */
  if (!strcmp(TEN_DWI_GAGE_KIND_NAME, kind->name)) {
    if (tenDWMRIKeyValueParse(&ngrad, &nbmat, &bval, &skip, &skipNum, nin)) {
      airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble parsing DWI info:\n%s\n", me, err);
      airMopError(mop); return 1;
    }
    if (skipNum) {
      fprintf(stderr, "%s: sorry, can't do DWI skipping in tenDwiGage", me);
      airMopError(mop); return 1;
    }
    /* this could stand to use some more command-line arguments */
    if (tenDwiGageKindSet(kind, 50, 1, bval, 0.001, ngrad, nbmat,
                          tenEstimate1MethodLLS,
                          tenEstimate2MethodQSegLLS,
                          /* randSeed */ 7919)) {
      airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble parsing DWI info:\n%s\n", me, err);
      airMopError(mop); return 1;
    }
  }

  ansLen = kind->table[what].answerLength;

  /* for setting up pre-blurred scale-space samples */
  if (numSS) {
    ninSS = AIR_CAST(Nrrd **, calloc(numSS, sizeof(Nrrd *)));
    if (!ninSS) {
      fprintf(stderr, "%s: couldn't allocate ninSS", me);
      airMopError(mop); return 1;
    }
    for (ninSSIdx=0; ninSSIdx<numSS; ninSSIdx++) {
      ninSS[ninSSIdx] = nrrdNew();
      airMopAdd(mop, ninSS[ninSSIdx], (airMopper)nrrdNuke, airMopAlways);
    }
    if (gageStackBlur(ninSS, numSS,
                      nin, kind->baseDim, 
                      kSSblur, rangeSS[0], rangeSS[1],
                      nrrdBoundaryBleed, AIR_TRUE, verbose, NULL)) {
      airMopAdd(mop, err = biffGetDone(GAGE), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble pre-computing blurrings:\n%s\n", me, err);
      airMopError(mop); return 1;
    }
  }
  
  ctx = gageContextNew();
  airMopAdd(mop, ctx, AIR_CAST(airMopper, gageContextNix), airMopAlways);
  gageParmSet(ctx, gageParmGradMagCurvMin, gmc);
  gageParmSet(ctx, gageParmVerbose, verbose);
  gageParmSet(ctx, gageParmRenormalize, renorm ? AIR_TRUE : AIR_FALSE);
  gageParmSet(ctx, gageParmCheckIntegrals, AIR_TRUE);
  E = 0;
  if (!E) E |= !(pvl = gagePerVolumeNew(ctx, nin, kind));
  if (!E) E |= gageKernelSet(ctx, gageKernel00, k00->kernel, k00->parm);
  if (!E) E |= gageKernelSet(ctx, gageKernel11, k11->kernel, k11->parm); 
  if (!E) E |= gageKernelSet(ctx, gageKernel22, k22->kernel, k22->parm);
  if (numSS) {
    gagePerVolume **pvlSS;
    gageParmSet(ctx, gageParmStackUse, AIR_TRUE);
    gageParmSet(ctx, gageParmStackRenormalize,
                SSrenorm ? AIR_TRUE : AIR_FALSE);
    if (!E) E |= gageStackPerVolumeNew(ctx, &pvlSS,
                                       AIR_CAST(const Nrrd**, ninSS),
                                       numSS, kind);
    if (!E) airMopAdd(mop, pvlSS, (airMopper)airFree, airMopAlways);
    if (!E) E |= gageStackPerVolumeAttach(ctx, pvl, pvlSS, numSS, 
                                          rangeSS[0], rangeSS[1]);
    if (!E) E |= gageKernelSet(ctx, gageKernelStack, kSS->kernel, kSS->parm);
  } else {
    if (!E) E |= gagePerVolumeAttach(ctx, pvl);
  }
  if (!E) E |= gageQueryItemOn(ctx, pvl, what);
  if (!E) E |= gageUpdate(ctx);
  if (E) {
    airMopAdd(mop, err = biffGetDone(GAGE), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  /* test with original context */
  answer = gageAnswerPointer(ctx, pvl, what);
  if (lpld) {
    double *dout, xyzw[4];
    unsigned int vidx, ai;
    nout = nrrdNew();
    airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
    if (1 == ansLen) {
      E = nrrdAlloc_va(nout, nrrdTypeDouble, 1, 
                       AIR_CAST(size_t, lpld->xyzwNum));
    } else {
      E = nrrdAlloc_va(nout, nrrdTypeDouble, 2, 
                       AIR_CAST(size_t, ansLen),
                       AIR_CAST(size_t, lpld->xyzwNum));
    }
    if (E) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble:\n%s\n", me, err);
      airMopError(mop);
      return 1;
    }
    dout = AIR_CAST(double *, nout->data);
    for (vidx=0; vidx<lpld->xyzwNum; vidx++) {
      ELL_4V_COPY(xyzw, lpld->xyzw + 4*vidx);
      ELL_4V_HOMOG(xyzw, xyzw);
      if (gageProbeSpace(ctx, xyzw[0], xyzw[1], xyzw[2],
                         AIR_FALSE, AIR_TRUE)) {
        fprintf(stderr, "%s: trouble:\n%s\n(%d)\n",
                me, ctx->errStr, ctx->errNum);
        airMopError(mop);
        return 1;
      }
      for (ai=0; ai<ansLen; ai++) {
        dout[ai + ansLen*vidx] = answer[ai];
      }
    }
    if (nrrdSave(outS, nout, NULL)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble saving output:\n%s\n", me, err);
      airMopError(mop);
      return 1;
    }
  } else if (lineStepNum) {
    /* we're probing along a line */
    double *dout;
    float start[3], dir[3], end[3], lpos[3];
    unsigned int vidx, ai;
    nout = nrrdNew();
    airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
    if (1 == ansLen) {
      E = nrrdAlloc_va(nout, nrrdTypeDouble, 1, 
                       AIR_CAST(size_t, lineStepNum));
    } else {
      E = nrrdAlloc_va(nout, nrrdTypeDouble, 2, 
                       AIR_CAST(size_t, ansLen),
                       AIR_CAST(size_t, lineStepNum));
    }
    if (E) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble:\n%s\n", me, err);
      airMopError(mop);
      return 1;
    }
    dout = AIR_CAST(double *, nout->data);
    ELL_3V_COPY(start, pos);
    if (lineInfo[3]) {
      float tmp;
      /* stepping along vector */
      ELL_3V_COPY(dir, 0 + lineInfo);
      ELL_3V_SET(end, AIR_NAN, AIR_NAN, AIR_NAN);
      ELL_3V_NORM(dir, dir, tmp);
      if (!tmp) {
        fprintf(stderr, "%s: requested vector stepping, but vlen = 0", me);
        airMopError(mop);
        return 1;
      }
      ELL_3V_SCALE(dir, lineInfo[3], dir);
    } else {
      /* stepping between points */
      ELL_3V_SET(dir, AIR_NAN, AIR_NAN, AIR_NAN);
      ELL_3V_COPY(end, 0 + lineInfo);
    }
    for (vidx=0; vidx<lineStepNum; vidx++) {
      if (lineInfo[3]) {
        ELL_3V_SCALE_ADD2(lpos, 1, start, vidx, dir);
      } else {
        ELL_3V_AFFINE(lpos, 0, vidx, lineStepNum-1, start, end);
      }
      if (gageProbeSpace(ctx, lpos[0], lpos[1], lpos[2],
                         AIR_TRUE, AIR_FALSE)) {
        fprintf(stderr, "%s: trouble:\n%s\n(%d)\n",
                me, ctx->errStr, ctx->errNum);
        airMopError(mop);
        return 1;
      }
      for (ai=0; ai<ansLen; ai++) {
        dout[ai + ansLen*vidx] = answer[ai];
      }
    }
    if (nrrdSave(outS, nout, NULL)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble saving output:\n%s\n", me, err);
      airMopError(mop);
      return 1;
    }
  } else {
    E = (numSS
         ? gageStackProbe(ctx, pos[0], pos[1], pos[2], idxSS)
         : gageProbe(ctx, pos[0], pos[1], pos[2]));
    if (E) {
      fprintf(stderr, "%s: trouble:\n%s\n(%d)\n",
              me, ctx->errStr, ctx->errNum);
      airMopError(mop);
      return 1;
    }
    printf("%s: %s(%g,%g,%g) = ", me,
           airEnumStr(kind->enm, what), pos[0], pos[1], pos[2]);
    printans(stdout, answer, ansLen);
    printf("\n");
  }

  airMopOkay(mop);
  return 0;
}
