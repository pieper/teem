/*
  Teem: Tools to process and visualize scientific data and images              
  Copyright (C) 2011, 2010, 2009  University of Chicago
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

#include "nrrd.h"
#include "privateNrrd.h"

static FILE *_fileSave = NULL;

int
_nrrdEncodingAscii_available(void) {

  return AIR_TRUE;
}

int
_nrrdEncodingAscii_read(FILE *file, void *_data, size_t elNum,
                        Nrrd *nrrd, NrrdIoState *nio) {
  static const char me[]="_nrrdEncodingAscii_read";
  char numbStr[AIR_STRLEN_HUGE];  /* HEY: fix this */
  char *nstr;
  size_t I;
  char *data;
  int tmp;

  AIR_UNUSED(nio);
  _fileSave = file;
  if (nrrdTypeBlock == nrrd->type) {
    biffAddf(NRRD, "%s: can't read nrrd type %s from %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock),
             nrrdEncodingAscii->name);
    return 1;
  }
  data = (char*)_data;
  I = 0;
  while (I < elNum) {
    /* HEY: we can easily suffer here from a standard buffer overflow problem;
       this was a source of a mysterious unu crash:
         echo "0 0 0 0 1 0 0 0 0" \
          | unu reshape -s 9 1 1 \
          | unu pad -min 0 0 0 -max 8 8 8 \
          | unu make -s 9 9 9 -t float -e ascii -ls 9 \
            -spc LPS -orig "(0,0,0)" -dirs "(1,0,0) (0,1,0) (0,0,1)"
       This particular case is resolved by changing AIR_STRLEN_HUGE
       to AIR_STRLEN_HUGE*100, but the general problem remains.  This
       motivated adding the memory corruption test; sadly once that has
       happened biffAddf also crashed */
    if (1 != fscanf(file, "%s", numbStr)) {
      biffAddf(NRRD, "%s: couldn't parse element " _AIR_SIZE_T_CNV
               " of " _AIR_SIZE_T_CNV, me, I+1, elNum);
      return 1;
    }
    if (file != _fileSave) {
      fprintf(stderr, "%s: sorry, memory corruption detected, bye.\n", me);
      exit(1);
    }
    if (!strcmp(",", numbStr)) {
      /* its an isolated comma, not a value, pass over this */
      continue;
    }
    /* get past any commas prefixing a number (without space) */
    nstr = numbStr + strspn(numbStr, ",");
    if (nrrd->type >= nrrdTypeInt) {
      /* sscanf supports putting value directly into this type */
      if (1 != airSingleSscanf(nstr, nrrdTypePrintfStr[nrrd->type], 
                               (void*)(data + I*nrrdElementSize(nrrd)))) {
        biffAddf(NRRD, "%s: couldn't parse %s " _AIR_SIZE_T_CNV
                 " of " _AIR_SIZE_T_CNV " (\"%s\")", me,
                 airEnumStr(nrrdType, nrrd->type),
                 I+1, elNum, nstr);
        return 1;
      }
    } else {
      /* sscanf value into an int first */
      if (1 != airSingleSscanf(nstr, "%d", &tmp)) {
        biffAddf(NRRD, "%s: couldn't parse element " _AIR_SIZE_T_CNV
                 " of " _AIR_SIZE_T_CNV " (\"%s\")",
                 me, I+1, elNum, nstr);
        return 1;
      }
      nrrdIInsert[nrrd->type](data, I, tmp);
    }
    I++;
  }
  
  return 0;
}

int
_nrrdEncodingAscii_write(FILE *file, const void *_data, size_t elNum,
                         const Nrrd *nrrd, NrrdIoState *nio) {
  static const char me[]="_nrrdEncodingAscii_write";
  char buff[AIR_STRLEN_MED];
  size_t bufflen, linelen;
  const char *data;
  size_t I;
  
  if (nrrdTypeBlock == nrrd->type) {
    biffAddf(NRRD, "%s: can't write nrrd type %s to %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock),
             nrrdEncodingAscii->name);
    return 1;
  }
  data = (char*)_data;
  linelen = 0;
  for (I=0; I<elNum; I++) {
    nrrdSprint[nrrd->type](buff, data);
    if (1 == nrrd->dim) {
      fprintf(file, "%s\n", buff);
    } else if (nrrd->dim == 2 
               && nrrd->axis[0].size <= nio->valsPerLine) {
      fprintf(file, "%s%c", buff,
              (I+1)%(nrrd->axis[0].size) ? ' ' : '\n');
    } else {
      bufflen = strlen(buff);
      if (linelen+bufflen+1 <= nio->charsPerLine) {
        fprintf(file, "%s%s", I ? " " : "", buff);
        linelen += (I ? 1 : 0) + bufflen;
      } else {
        fprintf(file, "\n%s", buff);
        linelen = bufflen;
      }
    }
    data += nrrdElementSize(nrrd);
  }
  /* just to be sure, we always end with a carraige return */
  fprintf(file, "\n");
  
  return 0;
}

const NrrdEncoding
_nrrdEncodingAscii = {
  "ASCII",      /* name */
  "ascii",      /* suffix */
  AIR_FALSE,   /* endianMatters */
  AIR_FALSE,   /* isCompression */
  _nrrdEncodingAscii_available,
  _nrrdEncodingAscii_read,
  _nrrdEncodingAscii_write
};

const NrrdEncoding *const
nrrdEncodingAscii = &_nrrdEncodingAscii;
