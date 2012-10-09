/*
  Teem: Tools to process and visualize scientific data and images              
  Copyright (C) 2012, 2011, 2010, 2009  University of Chicago
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

#include "teem/nrrd.h"

#define BKEY "tskip"


/* 
** Tests: 
** nrrdLoad with positive and negative byte skipping on data read,
** with nrrdEncodingRaw
*/

static const char *tskipInfo = "for skipping in nrrd data";

int
main(int argc, const char **argv) {
  /* stock variables */
  const char *me;
  hestOpt *hopt=NULL;
  hestParm *hparm;
  airArray *mop;
  /* variables specific to this program */
  int negskip;
  Nrrd *nref, *nin;
  size_t *size, ii, nn, pad[2];
  unsigned int axi, refCRC, gotCRC, sizeNum;
  char *berr, *outS[2], stmp[AIR_STRLEN_SMALL];
  airRandMTState *rng;
  unsigned int seed, *rdata;
  FILE *fout;

  /* start-up */
  AIR_UNUSED(argc);
  mop = airMopNew();
  hparm = hestParmNew();
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);
  me = argv[0];
  /* learn things from hest */
  hestOptAdd(&hopt, "seed", "N", airTypeUInt, 1, 1, &seed, "42",
             "seed for RNG");
  hestOptAdd(&hopt, "s", "sz0", airTypeSize_t, 1, -1, &size, NULL,
             "sizes of desired output", &sizeNum);
  hestOptAdd(&hopt, "p", "pb pa", airTypeSize_t, 2, 2, pad, "0 0",
             "bytes of padding before, and after, the data segment "
             "in the written data");
  hestOptAdd(&hopt, "ns", "bool", airTypeInt, 0, 0, &negskip, NULL,
             "skipping should be relative to end of file");
  hestOptAdd(&hopt, "o", "out.data out.nhdr", airTypeString, 2, 2,
             outS, NULL, "output filenames of data and header");
  hestParseOrDie(hopt, argc-1, argv+1, hparm, me, tskipInfo,
                 AIR_TRUE, AIR_TRUE, AIR_TRUE);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);
  
  /* generate reference nrrd data */
  nref = nrrdNew();
  airMopAdd(mop, nref, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdMaybeAlloc_nva(nref, nrrdTypeUInt, sizeNum, size)) {
    airMopAdd(mop, berr=biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error allocating data: %s\n", me, berr);
    airMopError(mop); return 1;
  }
  rng = airRandMTStateNew(seed);
  airMopAdd(mop, rng, (airMopper)airRandMTStateNix, airMopAlways);
  nn = nrrdElementNumber(nref);
  rdata = AIR_CAST(unsigned int *, nref->data);
  for (ii=0; ii<nn; ii++) {
    rdata[ii] = airUIrandMT_r(rng);
  }
  refCRC = nrrdCRC32(nref);
  printf("crc = %u\n", refCRC);

  /* write padded data */
  if (!(fout = fopen(outS[0], "wb"))) {
    fprintf(stderr, "%s: couldn't open %s for writing: %s\n", me,
            outS[0], strerror(errno));
    airMopError(mop); return 1;
  }
  airMopAdd(mop, fout, (airMopper)airFclose, airMopAlways);
  for (ii=0; ii<pad[0]; ii++) {
    if (EOF == fputc(1, fout)) {
      fprintf(stderr, "%s: error doing pre-padding\n", me);
      airMopError(mop); return 1;
    }
  }
  if (nn != fwrite(nref->data, nrrdElementSize(nref), nn, fout)) {
    fprintf(stderr, "%s: error writing data\n", me);
    airMopError(mop); return 1;
  }
  for (ii=0; ii<pad[1]; ii++) {
    if (EOF == fputc(2, fout)) {
      fprintf(stderr, "%s: error doing post-padding\n", me);
      airMopError(mop); return 1;
    }
  }
  airMopSingleOkay(mop, fout);
  airMopSingleOkay(mop, nref); nref = NULL;

  /* write header; for now just writing the header directly */
  if (!(fout = fopen(outS[1], "w"))) {
    fprintf(stderr, "%s: couldn't open %s for writing: %s\n", me,
            outS[1], strerror(errno));
    airMopError(mop); return 1;
  }
  airMopAdd(mop, fout, (airMopper)airFclose, airMopAlways);
  fprintf(fout, "NRRD0005\n");
  fprintf(fout, "type: unsigned int\n");
  fprintf(fout, "dimension: %u\n", sizeNum);
  fprintf(fout, "sizes:");
  for (axi=0; axi<sizeNum; axi++) {
    fprintf(fout, " %s", airSprintSize_t(stmp, size[axi]));
  }
  fprintf(fout, "\n");
  fprintf(fout, "endian: %s\n", airEnumStr(airEndian, airMyEndian()));
  fprintf(fout, "encoding: %s\n", airEnumStr(nrrdEncodingType,
                                             nrrdEncodingTypeRaw));
  if (!negskip) {
    if (pad[0]) {
      fprintf(fout, "byte skip: %s\n", airSprintSize_t(stmp, pad[0]));
    }
  } else {
    fprintf(fout, "byte skip: -%s\n", airSprintSize_t(stmp, pad[1]+1));
  }
  fprintf(fout, "data file: %s\n", outS[0]);
  airMopSingleOkay(mop, fout);
  
  /* read it in, make sure it checks out */
  nin = nrrdNew();
  airMopAdd(mop, nin, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdLoad(nin, outS[1], NULL)) {
    airMopAdd(mop, berr=biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error reading back in: %s\n", me, berr);
    airMopError(mop); return 1;
  }
  gotCRC = nrrdCRC32(nin);
  if (refCRC != gotCRC) {
    fprintf(stderr, "%s: got CRC %u but wanted %u\n", me, gotCRC, refCRC);
    airMopError(mop); return 1;
  }
  printf("(all ok)\n");

  /* HEY: to test gzip reading, we really want to do a system call to
     gzip compress the data, and write a new header to point to the
     compressed data, and make sure we can read in that just the same */

  airMopOkay(mop);
  return 0;
}
