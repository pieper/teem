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


#include "../air.h"

char *me;

int
main(int argc, char *argv[]) {
  char *fS, buff[128];
  float f, sf;
  double d;
  int ret;

  me = argv[0];
  if (2 != argc) {
    fprintf(stderr, "usage: %s <float>\n", me);
    exit(1);
  }
  fS = argv[1];

  ret = sscanf(fS, "%f", &sf);
  if (!ret) {
    printf("%s: sscanf(%s, \"%%f\") failed\n", me, fS);
    printf("\n");
  }
  if (1 != airSingleSscanf(fS, "%f", &f)) {
    fprintf(stderr, "%s: couldn't parse \"%s\" as float\n", me, fS);
    exit(1);
  }
  if (ret && (sf != f)) {
    printf("%s: sscanf result (%f) != airSingleSscanf (%f) !!!\n", me, sf, f);
    printf("\n");
  }
  d = f;
  airSinglePrintf(NULL, buff, "%f", f);
  printf("%s: printf/airSinglePrintf as float:\n%f\n%s\n", me, f, buff);
  airSinglePrintf(NULL, buff, "%f", d);
  printf("\n");
  printf("%s: printf/airSinglePrintf as double:\n%f\n%s\n", me, d, buff);
  printf("\n");
  printf("%s: airFPFprintf_f:\n", me);
  airFPFprintf_f(stderr, f);
  exit(0);
}
