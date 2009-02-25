/*
  Teem: Tools to process and visualize scientific data and images              
  Copyright (C) 2008, 2007, 2006, 2005  Gordon Kindlmann
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

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "pull.h"
#include "privatePull.h"

#define SPRING    "spring"
#define GAUSS     "gauss"
#define GAUSSHACK "ghack"
#define COTAN     "cotan"
#define CUBIC     "cubic"
#define QUARTIC   "quartic"
#define CWELL     "cwell"
#define ZERO      "zero"

char
_pullEnergyTypeStr[PULL_ENERGY_TYPE_MAX+1][AIR_STRLEN_SMALL] = {
  "(unknown_energy)",
  SPRING,
  GAUSS,
  GAUSSHACK,
  COTAN,
  CUBIC,
  QUARTIC,
  CWELL,
  ZERO
};

char
_pullEnergyTypeDesc[PULL_ENERGY_TYPE_MAX+1][AIR_STRLEN_MED] = {
  "unknown_energy",
  "Hooke's law-based potential, with a tunable region of attraction",
  "Gaussian potential",
  "like a Gaussian, but a lot wider",
  "Cotangent-based potential (from Meyer et al. SMI '05)",
  "Cubic thing",
  "Quartic thing",
  "Piecewice cubic with tunable well location and depth",
  "no energy"
};

airEnum
_pullEnergyType = {
  "energy",
  PULL_ENERGY_TYPE_MAX,
  _pullEnergyTypeStr,  NULL,
  _pullEnergyTypeDesc,
  NULL, NULL,
  AIR_FALSE
};
airEnum *
pullEnergyType = &_pullEnergyType;

/* ----------------------------------------------------------------
** ------------------------------ UNKNOWN -------------------------
** ----------------------------------------------------------------
*/
double
_pullEnergyUnknownEval(double *denr, double dist, const double *parm) {
  char me[]="_pullEnergyUnknownEval";

  AIR_UNUSED(dist);
  AIR_UNUSED(parm);
  *denr = AIR_NAN;
  fprintf(stderr, "%s: ERROR- using unknown energy.\n", me);
  return AIR_NAN;
}

pullEnergy
_pullEnergyUnknown = {
  "unknown",
  0,
  _pullEnergyUnknownEval
};
const pullEnergy *const
pullEnergyUnknown = &_pullEnergyUnknown;

/* ----------------------------------------------------------------
** ------------------------------ SPRING --------------------------
** ----------------------------------------------------------------
** 1 parms:
** parm[0]: width of pull region.  Used to be width beyond 1.0, but 
** now things are scrunched to fit both repelling and attractive
** region inside [0,1]
**
** learned: "1/2" is not 0.5 !!!!!
*/
double
_pullEnergySpringEval(double *denr, double dist, const double *parm) {
  /* char me[]="_pullEnergySpringEval"; */
  double enr, xx, pull;

  pull = parm[0];
  /* support used to be [0,1 + pull], but now is scrunched to [0,1],
     so hack "dist" to match old parameterization */
  dist = AIR_AFFINE(0, dist, 1, 0, 1+pull);
  xx = dist - 1.0;
  if (xx > pull) {
    enr = 0;
    *denr = 0;
  } else if (xx > 0) {
    enr = xx*xx*(xx*xx/(4*pull*pull) - 2*xx/(3*pull) + 1.0/2.0);
    *denr = xx*(xx*xx/(pull*pull) - 2*xx/pull + 1);
  } else {
    enr = xx*xx/2;
    *denr = xx;
  }
  /*
  if (!AIR_EXISTS(ret)) {
    printf("!%s: dist=%g, pull=%g, blah=%d --> ret=%g\n",
           me, dist, pull, blah, ret);
  }
  */
  return enr;
}

const pullEnergy
_pullEnergySpring = {
  SPRING,
  1,
  _pullEnergySpringEval
};
const pullEnergy *const
pullEnergySpring = &_pullEnergySpring;

/* ----------------------------------------------------------------
** ------------------------------ GAUSS --------------------------
** ----------------------------------------------------------------
** 0 parms: for simplicity we're now always cutting off at 4 sigmas
*/
/* HEY: copied from teem/src/nrrd/kernel.c */
#define _GAUSS(x, sig, cut) ( \
   x >= sig*cut ? 0           \
   : exp(-x*x/(2.0*sig*sig)))

#define _DGAUSS(x, sig, cut) ( \
   x >= sig*cut ? 0            \
   : -exp(-x*x/(2.0*sig*sig))*(x/(sig*sig)))

double
_pullEnergyGaussEval(double *denr, double dist, const double *parm) {

  AIR_UNUSED(parm);
  *denr = _DGAUSS(dist, 0.25, 4);
  return _GAUSS(dist, 0.25, 4);
}

const pullEnergy
_pullEnergyGauss = {
  GAUSS,
  0,
  _pullEnergyGaussEval
};
const pullEnergy *const
pullEnergyGauss = &_pullEnergyGauss;

/* ----------------------------------------------------------------
** ---------------------------- GAUSSHACK -------------------------
** ----------------------------------------------------------------
** 0 parms: for simplicity we're now always cutting off at 4 sigmas
*/
/* HEY: copied from teem/src/nrrd/kernel.c */
#define _GAUSSHACK(x, sig, cut) ( \
   x >= sig*cut ? 0           \
   : exp(-x*x*x*x*x*x/(2.0*sig*sig)))

#define _DGAUSSHACK(x, sig, cut) ( \
   x >= sig*cut ? 0            \
   : -exp(-x*x*x*x*x*x/(2.0*sig*sig))*(3*x*x*x*x*x/(sig*sig)))

double
_pullEnergyGaussHackEval(double *denr, double dist, const double *parm) {

  AIR_UNUSED(parm);
  *denr = _DGAUSSHACK(dist, 0.25, 4);
  return _GAUSSHACK(dist, 0.25, 4);
}

const pullEnergy
_pullEnergyGaussHack = {
  GAUSSHACK,
  0,
  _pullEnergyGaussHackEval
};
const pullEnergy *const
pullEnergyGaussHack = &_pullEnergyGaussHack;

/* ----------------------------------------------------------------
** ------------------------------ COTAN ---------------------------
** ----------------------------------------------------------------
** 0 parms!
*/
double
_pullEnergyCotanEval(double *denr, double dist, const double *parm) {
  double pot, cc, enr;

  AIR_UNUSED(parm);
  pot = AIR_PI/2.0;
  cc = 1.0/(FLT_MIN + tan(dist*pot));
  enr = dist > 1 ? 0 : cc + dist*pot - pot;
  *denr = dist > 1 ? 0 : -cc*cc*pot;
  return enr;
}

const pullEnergy
_pullEnergyCotan = {
  COTAN,
  0,
  _pullEnergyCotanEval
};
const pullEnergy *const
pullEnergyCotan = &_pullEnergyCotan;

/* ----------------------------------------------------------------
** ------------------------------ CUBIC ---------------------------
** ----------------------------------------------------------------
** 0 parms!
*/
double
_pullEnergyCubicEval(double *denr, double dist, const double *parm) {
  double omr, enr;

  AIR_UNUSED(parm);
  if (dist <= 1) {
    omr = 1 - dist;
    enr = omr*omr*omr;
    *denr = -3*omr*omr;
  } else {
    enr = *denr = 0;
  }
  return enr;
}

const pullEnergy
_pullEnergyCubic = {
  CUBIC,
  0,
  _pullEnergyCubicEval
};
const pullEnergy *const
pullEnergyCubic = &_pullEnergyCubic;

/* ----------------------------------------------------------------
** ----------------------------- QUARTIC --------------------------
** ----------------------------------------------------------------
** 0 parms!
*/
double
_pullEnergyQuarticEval(double *denr, double dist, const double *parm) {
  double omr, enr;

  AIR_UNUSED(parm);
  if (dist <= 1) {
    omr = 1 - dist;
    enr = 2.132*omr*omr*omr*omr;
    *denr = -4*2.132*omr*omr*omr;
  } else {
    enr = *denr = 0;
  }
  return enr;
}

const pullEnergy
_pullEnergyQuartic = {
  QUARTIC,
  0,
  _pullEnergyQuarticEval
};
const pullEnergy *const
pullEnergyQuartic = &_pullEnergyQuartic;

/* ----------------------------------------------------------------
** ------------------ tunable piece-wise cubic --------------------
** ----------------------------------------------------------------
** 2 parm: wellX, wellY
*/
double
_pullEnergyCubicWellEval(double *denr, double x, const double *parm) {
  double a, b, c, d, e, wx, wy, enr;

  wx = parm[0];
  wy = parm[1];
  a = (3*(-1 + wy))/wx;
  b = (-3*(-1 + wy))/(wx*wx);
  c = -(1 - wy)/(wx*wx*wx);
  d = (-3*wy)/((wx-1)*(wx-1));
  e = (-2*wy)/((wx-1)*(wx-1)*(wx-1));
  if (x < wx) {
    enr = 1 + x*(a + x*(b + c*x));
    *denr = a + x*(2*b + 3*c*x);
  } else if (x < 1) {
    double _x;
    _x = x - wx;
    enr = wy + _x*_x*(d + e*_x);
    *denr = _x*(2*d + 3*e*_x);
  } else {
    enr = 0;
    *denr = 0;
  }
  return enr;
}

const pullEnergy
_pullEnergyCubicWell = {
  CWELL,
  2,
  _pullEnergyCubicWellEval
};
const pullEnergy *const
pullEnergyCubicWell = &_pullEnergyCubicWell;

/* ----------------------------------------------------------------
** ------------------------------- ZERO ---------------------------
** ----------------------------------------------------------------
** 0 parms:
*/
double
_pullEnergyZeroEval(double *denr, double dist, const double *parm) {

  AIR_UNUSED(dist);
  AIR_UNUSED(parm);
  *denr = 0;
  return 0;
}

const pullEnergy
_pullEnergyZero = {
  ZERO,
  0,
  _pullEnergyZeroEval
};
const pullEnergy *const
pullEnergyZero = &_pullEnergyZero;

/* ----------------------------------------------------------------
** ----------------------------------------------------------------
** ----------------------------------------------------------------
*/

const pullEnergy *const pullEnergyAll[PULL_ENERGY_TYPE_MAX+1] = {
  &_pullEnergyUnknown,   /* 0 */
  &_pullEnergySpring,    /* 1 */
  &_pullEnergyGauss,     /* 2 */
  &_pullEnergyGaussHack, /* 3 */
  &_pullEnergyCotan,     /* 4 */
  &_pullEnergyCubic,     /* 5 */
  &_pullEnergyQuartic,   /* 6 */
  &_pullEnergyCubicWell, /* 7 */
  &_pullEnergyZero       /* 8 */
};

pullEnergySpec *
pullEnergySpecNew() {
  pullEnergySpec *ensp;
  int pi;

  ensp = (pullEnergySpec *)calloc(1, sizeof(pullEnergySpec));
  if (ensp) {
    ensp->energy = pullEnergyUnknown;
    for (pi=0; pi<PULL_ENERGY_PARM_NUM; pi++) {
      ensp->parm[pi] = AIR_NAN;
    }
  }
  return ensp;
}

int
pullEnergySpecSet(pullEnergySpec *ensp, const pullEnergy *energy,
                  const double parm[PULL_ENERGY_PARM_NUM]) {
  char me[]="pullEnergySpecSet", err[BIFF_STRLEN];
  unsigned int pi;

  if (!( ensp && energy )) {
    sprintf(err, "%s: got NULL pointer", me); 
    biffAdd(PULL, err); return 1;
  }
  if (ensp && energy && parm) {
    ensp->energy = energy;
    for (pi=0; pi<PULL_ENERGY_PARM_NUM; pi++) {
      ensp->parm[pi] = parm[pi];
    }
  }
  return 0;
}

pullEnergySpec *
pullEnergySpecNix(pullEnergySpec *ensp) {

  airFree(ensp);
  return NULL;
}

int
pullEnergySpecParse(pullEnergySpec *ensp, const char *_str) {
  char me[]="pullEnergySpecParse", err[BIFF_STRLEN];
  char *str, *col, *_pstr, *pstr;
  int etype;
  unsigned int pi, haveParm;
  airArray *mop;
  double pval;

  if (!( ensp && _str )) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(PULL, err); return 1;
  }

  /* see if its the name of something that needs no parameters */
  etype = airEnumVal(pullEnergyType, _str);
  if (pullEnergyTypeUnknown != etype) {
    /* the string is the name of some energy */
    ensp->energy = pullEnergyAll[etype];
    if (0 != ensp->energy->parmNum) {
      sprintf(err, "%s: need %u parms for %s energy, but got none", me,
              ensp->energy->parmNum, ensp->energy->name);
      biffAdd(PULL, err); return 1;
    }
    /* the energy needs 0 parameters */
    for (pi=0; pi<PULL_ENERGY_PARM_NUM; pi++) {
      ensp->parm[pi] = AIR_NAN;
    }
    return 0;
  }

  /* start parsing parms after ':' */
  mop = airMopNew();
  str = airStrdup(_str);
  airMopAdd(mop, str, (airMopper)airFree, airMopAlways);
  col = strchr(str, ':');
  if (!col) {
    sprintf(err, "%s: either \"%s\" is not a recognized energy, "
            "or it is an energy with parameters, and there's no "
            "\":\" separator to indicate parameters", me, str);
    biffAdd(PULL, err); airMopError(mop); return 1;
  }
  *col = '\0';
  etype = airEnumVal(pullEnergyType, str);
  if (pullEnergyTypeUnknown == etype) {
    sprintf(err, "%s: didn't recognize \"%s\" as a %s", me,
            str, pullEnergyType->name);
    biffAdd(PULL, err); airMopError(mop); return 1;
  }

  ensp->energy = pullEnergyAll[etype];
  if (0 == ensp->energy->parmNum) {
    sprintf(err, "%s: \"%s\" energy has no parms, but got something", me,
            ensp->energy->name);
    biffAdd(PULL, err); return 1;
  }

  _pstr = pstr = col+1;
  /* code lifted from teem/src/nrrd/kernel.c, should probably refactor... */
  for (haveParm=0; haveParm<ensp->energy->parmNum; haveParm++) {
    if (!pstr) {
      break;
    }
    if (1 != sscanf(pstr, "%lg", &pval)) {
      sprintf(err, "%s: trouble parsing \"%s\" as double (in \"%s\")",
              me, _pstr, _str);
      biffAdd(PULL, err); airMopError(mop); return 1;
    }
    ensp->parm[haveParm] = pval;
    if ((pstr = strchr(pstr, ','))) {
      pstr++;
      if (!*pstr) {
        sprintf(err, "%s: nothing after last comma in \"%s\" (in \"%s\")",
                me, _pstr, _str);
        biffAdd(PULL, err); airMopError(mop); return 1;
      }
    }
  }
  /* haveParm is now the number of parameters that were parsed. */
  if (haveParm < ensp->energy->parmNum) {
    sprintf(err, "%s: parsed only %u of %u required parms (for %s energy)"
            "from \"%s\" (in \"%s\")",
            me, haveParm, ensp->energy->parmNum,
            ensp->energy->name, _pstr, _str);
    biffAdd(PULL, err); airMopError(mop); return 1;
  } else {
    if (pstr) {
      sprintf(err, "%s: \"%s\" (in \"%s\") has more than %u doubles",
              me, _pstr, _str, ensp->energy->parmNum);
      biffAdd(PULL, err); airMopError(mop); return 1;
    }
  }
  
  airMopOkay(mop);
  return 0;
}

int
_pullHestEnergyParse(void *ptr, char *str, char err[AIR_STRLEN_HUGE]) {
  pullEnergySpec **enspP;
  char me[]="_pullHestForceParse", *perr;

  if (!(ptr && str)) {
    sprintf(err, "%s: got NULL pointer", me);
    return 1;
  }
  enspP = (pullEnergySpec **)ptr;
  *enspP = pullEnergySpecNew();
  if (pullEnergySpecParse(*enspP, str)) {
    perr = biffGetDone(PULL);
    strncpy(err, perr, AIR_STRLEN_HUGE-1);
    free(perr);
    return 1;
  }
  return 0;
}

hestCB
_pullHestEnergySpec = {
  sizeof(pullEnergySpec*),
  "energy specification",
  _pullHestEnergyParse,
  (airMopper)pullEnergySpecNix
};

hestCB *
pullHestEnergySpec = &_pullHestEnergySpec;
