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

char *permuteName = "permute";
#define INFO "Permute scan-line ordering of axes"
char *permuteInfo = INFO;
char *permuteInfoL = (INFO
		      ". The permutation gives the new ordering of the old "
		      "axes (in 0-based numbering). For example, the "
		      "permutation 0->1,\t1->2,\t2->0 would be \"2 0 1\".");

int
permuteMain(int argc, char **argv, char *me) {
  hestOpt *opt = NULL;
  char *out, *err;
  Nrrd *nin, *nout;
  int *perm, permLen;
  airArray *mop;

  OPT_ADD_NIN(nin, "input nrrd");
  hestOptAdd(&opt, "p", "ax0 ax1", airTypeInt, 1, -1, &perm, NULL,
	     "new axis ordering", &permLen);
  OPT_ADD_NOUT(out, "output nrrd");

  mop = airMopInit();
  airMopAdd(mop, opt, (airMopper)hestOptFree, airMopAlways);

  USAGE(permuteInfoL);
  PARSE();
  airMopAdd(mop, opt, (airMopper)hestParseFree, airMopAlways);

  nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);

  if (!( permLen == nin->dim )) {
    fprintf(stderr,
	    "%s: # axes in permutation (%d) != nrrd dim (%d)\n",
	    me, permLen, nin->dim);
    airMopError(mop);
    return 1;
  }

  if (nrrdPermuteAxes(nout, nin, perm)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error permuting nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  SAVE(nout, NULL);

  airMopOkay(mop);
  return 0;
}
