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

#include "echo.h"

#define NEW_TMPL(TYPE, BODY)                                     \
EchoLight##TYPE *                                                \
_echoLight##TYPE##_new(void) {                                   \
  int dummy=0;                                                   \
  EchoLight##TYPE *light;                                        \
                                                                 \
  light = (EchoLight##TYPE *)calloc(1, sizeof(EchoLight##TYPE)); \
  light->type = echoLight##TYPE;                                 \
  do { BODY dummy=dummy; } while (0);                            \
  return light;                                                  \
}

NEW_TMPL(Directional,dummy=dummy;) /* _echoLightDirectional_new */
NEW_TMPL(Area,                     /* _echoLightArea_new */
	 light->obj = NULL;
	 )

EchoLight_ *(*
_echoLightNew[ECHO_LIGHT_MAX+1])(void) = {
  NULL,
  (EchoLight_ *(*)(void))_echoLightDirectional_new,
  (EchoLight_ *(*)(void))_echoLightArea_new
};

EchoLight_ *
echoLightNew(int type) {
  
  return _echoLightNew[type]();
}

/* ---------------------------------------------------- */

EchoLightArea *
_echoLightArea_nix(EchoLightArea *area) {
  
  /* ??? */
  free(area);
  return NULL;
}

EchoLight_ *(*
_echoLightNix[ECHO_LIGHT_MAX+1])(EchoLight_ *) = {
  NULL,
  (EchoLight_ *(*)(EchoLight_ *))airFree,
  (EchoLight_ *(*)(EchoLight_ *))_echoLightArea_nix
};

EchoLight_ *
echoLightNix(EchoLight_ *light) {

  return _echoLightNix[light->type](light);
}

airArray *
echoLightArrayNew() {
  airArray *ret;

  ret = airArrayNew(NULL, NULL, sizeof(EchoLight_ *), 1);
  airArrayPointerCB(ret, airNull, (void *(*)(void *))echoLightNix);
  return ret;
}

void
echoLightArrayAdd(airArray *lightArr, EchoLight_ *light) {
  int idx;
  
  if (!(lightArr && light))
    return;

  idx = airArrayIncrLen(lightArr, 1);
  ((EchoLight_ **)lightArr->data)[idx] = light;
}

airArray *
echoLightArrayNix(airArray *lightArr) {
  
  if (lightArr) {
    airArrayNuke(lightArr);
  }
  return NULL;
}

void
echoLightDirectionalSet(EchoLight_ *_light,
			echoCol_t r, echoCol_t g, echoCol_t b,
			echoPos_t x, echoPos_t y, echoPos_t z) {
  EchoLightDirectional *light;
  echoPos_t tmp;

  if (_light && echoLightDirectional == _light->type) {
    light = (EchoLightDirectional *)_light;
    ELL_3V_SET(light->col, r, g, b);
    ELL_3V_SET(light->dir, x, y, z);
    ELL_3V_NORM(light->dir, light->dir, tmp);
  }
}
  
void
echoLightAreaSet(EchoLight_ *_light, EchoObject *obj) {
  EchoLightArea *light;

  if (_light && echoLightArea == _light->type) {
    light = (EchoLightArea *)_light;
    light->obj = obj;
  }
}
