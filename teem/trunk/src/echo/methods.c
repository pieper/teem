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

EchoParam *
echoParamNew(void) {
  EchoParam *param;
  
  param = (EchoParam *)calloc(1, sizeof(EchoParam));
  param->verbose = 1;
  param->jitter = echoJitterNone;
  param->samples = 1;
  param->shadow = AIR_TRUE;
  param->maxRecDepth = 5;
  param->reuseJitter = AIR_FALSE;
  param->permuteJitter = AIR_TRUE;
  param->amR = 1.0;
  param->amG = 1.0;
  param->amB = 1.0;
  param->mrR = 0.0;
  param->mrG = 0.0;
  param->mrB = 0.0;
  param->bgR = 0.0;
  param->bgG = 0.0;
  param->bgB = 0.0;
  param->gamma = 2.2;
  param->timeGamma = 6.0;
  param->renderLights = AIR_TRUE;
  param->renderBoxes = AIR_FALSE;
  param->refDistance = 1.0;
  param->areaLightHack = 35;
  param->seedRand = AIR_TRUE;

  /* these will have to be user-set */
  param->imgResU = param->imgResV = 0;
  param->aperture = AIR_NAN;

  return param;
}

EchoParam *
echoParamNix(EchoParam *param) {

  free(param);

  return NULL;
}

EchoGlobalState *
echoGlobalStateNew(void) {
  EchoGlobalState *state;
  
  state = (EchoGlobalState *)calloc(1, sizeof(EchoGlobalState));
  
  return state;
}

EchoGlobalState *
echoGlobalStateNix(EchoGlobalState *state) {

  free(state);
  return NULL;
}

EchoThreadState *
echoThreadStateNew(void) {
  EchoThreadState *state;
  
  state = (EchoThreadState *)calloc(1, sizeof(EchoThreadState));
  state->njitt = nrrdNew();
  state->nperm = nrrdNew();
  
  return state;
}

EchoThreadState *
echoThreadStateNix(EchoThreadState *state) {

  nrrdNuke(state->njitt);
  nrrdNuke(state->nperm);
  free(state->permBuff);
  free(state->chanBuff);
  free(state);
  return NULL;
}

limnCam *
echoLimnCamNew(void) {
  limnCam *cam;

  cam = limnCamNew();
  cam->eyeRel = AIR_FALSE;
  cam->leftHanded = AIR_FALSE;

  return cam;
}
