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
#include "privateEcho.h"

/*
** _echoLightColDir
**
** computes a light color and direction corresponding to a given
** sample of an area light
*/
void
_echoLightColDir(echoCol_t *lcol, echoPos_t ldir[3],
		 echoPos_t *distP, echoRTParm *parm,
		 echoObject *_light, int samp,
		 echoIntx *intx, echoThreadState *tstate) {
  echoPos_t at[3], *jitt, x, y;
  float colScale;
  echoRectangle *light;

  if (!( echoTypeRectangle == _light->type )) {
    fprintf(stderr, "%s: currently only support echoTypeRectangle lights", 
	    "_echoLightColDir");
    return;
  } 

  light = RECTANGLE(_light);
  jitt = ((echoPos_t *)tstate->njitt->data) + samp*2*ECHO_JITTABLE_NUM;
  x = jitt[0 + 2*echoJittableLight] + 0.5;
  y = jitt[1 + 2*echoJittableLight] + 0.5;
  ELL_3V_SCALEADD3(at, 1.0, light->origin, x, light->edge0, y, light->edge1);
  ELL_3V_SUB(ldir, at, intx->pos);
  *distP = ELL_3V_LEN(ldir);
  ELL_3V_SCALE(ldir, 1.0/(*distP), ldir);
  colScale = parm->refDistance/(*distP);
  colScale *= colScale*light->mat[echoMatterLightPower];
  lcol[0] = colScale * light->rgba[0];
  lcol[1] = colScale * light->rgba[1];
  lcol[2] = colScale * light->rgba[2];
  return;
}

void
_echoIntxColorUnknown(INTXCOLOR_ARGS) {
  
  fprintf(stderr, "%s: can't color intx with object with unset material\n",
	  "_echoIntxColorNone");
}

void
_echoIntxUVColor(echoCol_t *chan, echoIntx *intx) {
  int ui, vi, su, sv;
  unsigned char *tdata;
  echoCol_t *mat;

  mat = intx->obj->mat;
  if (intx->obj->ntext) {
    _echoRayIntxUV[intx->obj->type](intx);
    su = intx->obj->ntext->axis[1].size;
    sv = intx->obj->ntext->axis[2].size;
    AIR_INDEX(0.0, intx->u, 1.0, su, ui); ui = AIR_CLAMP(0, ui, su-1);
    AIR_INDEX(0.0, intx->v, 1.0, sv, vi); vi = AIR_CLAMP(0, vi, sv-1);
    tdata = intx->obj->ntext->data;
    chan[0] = intx->obj->rgba[0]*(tdata[0 + 4*(ui + su*vi)]/255.0);
    chan[1] = intx->obj->rgba[1]*(tdata[1 + 4*(ui + su*vi)]/255.0);
    chan[2] = intx->obj->rgba[2]*(tdata[2 + 4*(ui + su*vi)]/255.0);
    chan[3] = 1.0;
  } else {
    chan[0] = intx->obj->rgba[0];
    chan[1] = intx->obj->rgba[1];
    chan[2] = intx->obj->rgba[2];
    chan[3] = 1.0;
  }
}

void
_echoIntxColorPhong(INTXCOLOR_ARGS) {
  echoCol_t *mat,        /* pointer to object's material info */
    icol[4],             /* "intersection color" */
    oa,                  /* object opacity */
    d[3], s[3],          /* ambient + diffuse, specular */
    ka, kd, ks, sh,      /* phong parmeters */
    lcol[3];             /* light color */
  echoPos_t tmp,
    view[3],             /* unit-length view vector */
    norm[3],             /* unit-length normal */
    ldir[3],             /* unit-length light vector */
    refl[3],             /* unit-length reflection vector */
    ldist;               /* distance to light */
  int lt, qn;
  echoObject *light;
  echoRay shRay;
  echoIntx shIntx;
#if !ECHO_POS_FLOAT
  float norm_f[3];
#endif

  shRay.depth = 0;
  shRay.shadow = AIR_TRUE;

  mat = intx->obj->mat;
  oa = intx->obj->rgba[3];
  ka = mat[echoMatterPhongKa];
  kd = mat[echoMatterPhongKd];
  ks = mat[echoMatterPhongKs];
  sh = mat[echoMatterPhongSp];

  ELL_3V_NORM(norm, intx->norm, tmp);
  ELL_3V_NORM(view, intx->view, tmp);
  tmp = 2*ELL_3V_DOT(view, norm);
  ELL_3V_SCALEADD(refl, -1, view, tmp, norm);

  d[0] = ka*scene->ambi[0];
  d[1] = ka*scene->ambi[1];
  d[2] = ka*scene->ambi[2];
  if (scene->envmap) {
#if ECHO_POS_FLOAT
    qn = limnVtoQN[limnQN_16checker](norm);
#else
    ELL_3V_COPY(norm_f, norm);
    qn = limnVtoQN[limnQN_16checker](norm_f);
#endif
    d[0] += (ka+kd)*((float*)(scene->envmap->data))[0 + 3*qn];
    d[1] += (ka+kd)*((float*)(scene->envmap->data))[1 + 3*qn];
    d[2] += (ka+kd)*((float*)(scene->envmap->data))[2 + 3*qn];
  }
  s[0] = s[1] = s[2] = 0.0;
  ELL_3V_COPY(shRay.from, intx->pos);
  shRay.neer = ECHO_EPSILON;
  for (lt=0; lt<scene->litArr->len; lt++) {
    light = scene->lit[lt];
    _echoLightColDir(lcol, ldir, &ldist, parm,
		     light, samp, intx, tstate);
    tmp = ELL_3V_DOT(ldir, norm);
    if (tmp <= 0)
      continue;
    if (parm->doShadows) {
      ELL_3V_COPY(shRay.dir, ldir);
      shRay.faar = ldist;
      if (tstate->verbose) {
	printf("%d: from (%g,%g,%g) to (%g,%g,%g) for [%g,%g]\n",
	       lt, shRay.from[0], shRay.from[1], shRay.from[2],
	       shRay.dir[0], shRay.dir[1], shRay.dir[2],
	       shRay.neer, shRay.faar);
      }
      if (echoRayIntx(&shIntx, &shRay, scene, parm, tstate)) {
	/* the shadow ray hit something, nevermind */
	if (tstate->verbose) {
	  printf("       SHADOWED\n");
	}
	continue; /* to next light */
      }
      if (tstate->verbose) {
	printf(" I see the light\n");
      }
    }
    tmp *= kd;
    ELL_3V_SCALEADD(d, 1, d, tmp, lcol);
    /* NOTE: by including the specular stuff on this side of the
       "continue", we are disallowing specular highlights on the
       backsides of even semi-transparent surfaces. */
    if (ks) {
      tmp = ELL_3V_DOT(ldir, refl);
      if (tmp > 0) {
	tmp = ks*pow(tmp, sh);
	ELL_3V_SCALEADD(s, 1, s, tmp, lcol);
      }
    }
  }
  _echoIntxUVColor(icol, intx);
  chan[0] = icol[0]*d[0] + s[0];
  chan[1] = icol[1]*d[1] + s[1];
  chan[2] = icol[2]*d[2] + s[2];
  chan[3] = icol[3]*oa;
}

void
_echoFuzzifyNormal(echoPos_t norm[3], echoCol_t fuzz,
		   echoPos_t *jitt, echoPos_t view[3]) {
  float tmp, p0[3], p1[3], origNorm[3], j0, j1;
  int side, newside;

  /* we assume that the incoming normal is actually normalized */
  ELL_3V_COPY(origNorm, norm);
  side = (ELL_3V_DOT(view, origNorm) > 0 ? 1 : -1);
  ell3vPerp_f(p0, origNorm);
  ELL_3V_NORM(p0, p0, tmp);
  ELL_3V_CROSS(p1, p0, origNorm);
  j0 = fuzz*jitt[0];
  j1 = fuzz*jitt[1];
  ELL_3V_SCALEADD3(norm, 1.0, origNorm, j0, p0, j1, p1);
  newside = (ELL_3V_DOT(view, norm) > 0 ? 1 : -1);
  if (side != newside) {
    ELL_3V_SCALEADD3(norm, 1.0, origNorm, -j0, p0, -j1, p1);
  }
  ELL_3V_NORM(norm, norm, tmp);
}

void
_echoIntxColorMetal(INTXCOLOR_ARGS) {
  echoCol_t *mat,        /* pointer to object's material info */
    RS, RD,
    d[3],
    fuzz,
    lcol[3],
    rfCol[5];         /* color from reflected ray */
  echoPos_t tmp,
    view[3],          /* unit-length view vector */
    norm[3];          /* unit-length normal */
  echoPos_t *jitt,    /* fuzzy reflection jittering */
    ldir[3], ldist;
  double c;
  echoRay rfRay, shRay;
  echoIntx shIntx;
  int lt;
  /*
  echoLight_ *light;
  */

  mat = intx->obj->mat;

  if (0 && tstate->verbose) {
    printf("depth = %d, t = %g\n", intx->depth, intx->t);
  }
  ELL_3V_NORM(intx->norm, intx->norm, tmp);
  ELL_3V_NORM(view, intx->view, tmp);
  ELL_3V_COPY(norm, intx->norm);
  ELL_3V_COPY(rfRay.from, intx->pos);
  rfRay.neer = ECHO_EPSILON;
  rfRay.faar = ECHO_POS_MAX;
  rfRay.depth = intx->depth + 1;
  rfRay.shadow = AIR_FALSE;
  fuzz = mat[echoMatterMetalFuzzy];
  if (fuzz) {
    jitt = ((echoPos_t *)tstate->njitt->data
	    + 2*(samp*ECHO_JITTABLE_NUM + echoJittableNormalA));
    _echoFuzzifyNormal(norm, fuzz, jitt, view);
  }
  tmp = 2*ELL_3V_DOT(view, norm);
  ELL_3V_SCALEADD(rfRay.dir, -1.0, view, tmp, norm);
  echoRayColor(rfCol, samp, &rfRay, scene, parm, tstate);
  
  c = 1 - ELL_3V_DOT(norm, view);
  c = c*c*c*c*c;
  RS = mat[echoMatterMetalR0];
  RS = RS + (1 - RS)*c;
  RD = mat[echoMatterMetalKd]*(1-RS);
  d[0] = d[1] = d[2] = 0.0;
#if 0
  if (RD) {
    /* compute the diffuse component */
    ELL_3V_COPY(shRay.from, intx->pos);
    shRay.neer = ECHO_EPSILON;
    for (lt=0; lt<lightArr->len; lt++) {
      light = ((echoLight_ **)lightArr->data)[lt];
      _echoLightColDir[light->type](lcol, ldir, &ldist, parm,
				    light, samp, intx, tstate);
      tmp = ELL_3V_DOT(ldir, norm);
      if (tmp <= 0)
	continue;
      if (tstate->verbose) {
	printf("light %d: dot = %g\n", lt, tmp);
      }
      if (parm->doShadows) {
	ELL_3V_COPY(shRay.dir, ldir);
	shRay.faar = ldist;
	if (echoRayIntx(&shIntx, &shRay, parm, scene)) {
	  /* the shadow ray hit something, nevermind */
	  continue;
	}
      }
      ELL_3V_SCALEADD(d, 1, d, tmp, lcol);
    }
  }
#endif
  chan[0] = intx->obj->rgba[0]*(RD*d[0] + RS*rfCol[0]);
  chan[1] = intx->obj->rgba[1]*(RD*d[1] + RS*rfCol[1]);
  chan[2] = intx->obj->rgba[2]*(RD*d[2] + RS*rfCol[2]);
  chan[3] = 1.0;

  return;
}

/*
** n == 1, n_t == index 
*/
int
_echoRefract(echoPos_t T[3], echoPos_t V[3],
	     echoPos_t N[3], echoCol_t index) {
  echoPos_t cosTh, cosPh, sinPhsq, rad, tmp1, tmp2;

  cosTh = ELL_3V_DOT(V, N);
  sinPhsq = (1 - cosTh*cosTh)/(index*index);
  rad = 1 - sinPhsq;
  if (rad < 0) 
    return AIR_FALSE;
  /* else we do not have total internal reflection */
  cosPh = sqrt(rad);
  tmp1 = -1.0/index; tmp2 = cosTh/index - cosPh; 
  ELL_3V_SCALEADD(T, tmp1, V, tmp2, N);
  return AIR_TRUE;
}

void
_echoIntxColorGlass(INTXCOLOR_ARGS) {
  echoCol_t *mat,     /* pointer to object's material info */
    RS, R0,
    k[3], r, g, b,
    fuzz, index,
    rfCol[5],         /* color from reflected ray */
    trCol[5];         /* color from transmitted ray */
  echoPos_t           /* since I don't want to branch to call ell3vPerp_? */
    tmp,
    refl[3], tran[3],
    view[3],          /* unit-length view vector */
    norm[3];          /* unit-length normal */
  echoPos_t *jitt;    /* fuzzy reflection jittering */
  double c;
  echoRay trRay, rfRay;

  mat = intx->obj->mat;

  ELL_3V_NORM(intx->norm, intx->norm, tmp);
  ELL_3V_COPY(norm, intx->norm);
  ELL_3V_NORM(view, intx->view, tmp);
  ELL_3V_COPY(trRay.from, intx->pos);
  ELL_3V_COPY(rfRay.from, intx->pos);
  trRay.neer = rfRay.neer = ECHO_EPSILON;
  trRay.faar = rfRay.faar = ECHO_POS_MAX;
  trRay.depth = rfRay.depth = intx->depth + 1;
  trRay.shadow = rfRay.shadow = AIR_FALSE;
  fuzz = mat[echoMatterGlassFuzzy];
  index = mat[echoMatterGlassIndex];
  tmp = ELL_3V_DOT(view, norm);
  if (fuzz) {
    jitt = (echoPos_t *)tstate->njitt->data+ 2*samp*ECHO_JITTABLE_NUM;
    jitt += 2*(echoJittableNormalA + (tmp > 0));
    _echoFuzzifyNormal(norm, fuzz, jitt, view);
  }
  ELL_3V_SCALEADD(refl, -1, view, 2*tmp, norm);
  if (tstate->verbose) {
    printf("(glass; depth = %d); \n"
	   "    view = (%g,%g,%g); norm = (%g,%g,%g)\n"
	   "    view . norm = %g\n", 
	   intx->depth, view[0], view[1], view[2],
	   norm[0], norm[1], norm[2], tmp);
  }
  
  if (tmp > 0) {
    /* "d.n < 0": we're bouncing off the outside */
    _echoRefract(tran, view, norm, index);
    c = tmp;
    if (tstate->verbose) {
      printf("   outside bounce tran = (%g,%g,%g); c = %g\n",
	     tran[0], tran[1], tran[2], c);
    }
    ELL_3V_SET(k, 1, 1, 1);
  }
  else {
    /* we're bouncing off the inside */
    r = intx->obj->rgba[0];
    g = intx->obj->rgba[1];
    b = intx->obj->rgba[2];
    k[0] = r*exp((r-1)*intx->t);
    k[1] = g*exp((g-1)*intx->t);
    k[2] = b*exp((b-1)*intx->t);
    ELL_3V_SCALE(norm, -1, norm);
    if (_echoRefract(tran, view, norm, 1/index)) {
      c = -ELL_3V_DOT(tran, norm);
      if (tstate->verbose) {
	printf("   inside bounce tran = (%g,%g,%g); c = %g\n",
	       tran[0], tran[1], tran[2], c);
      }
    }
    else {
      /* holy moly, its total internal reflection time */
      ELL_3V_COPY(rfRay.dir, refl);
      echoRayColor(chan, samp, &rfRay, scene, parm, tstate);
      chan[0] *= k[0];
      chan[1] *= k[1];
      chan[2] *= k[2];
      return;
    }
  }
  R0 = (index - 1)/(index + 1);
  R0 *= R0;
  c = 1 - c;
  c = c*c*c*c*c;
  RS = R0 + (1-R0)*c;
  if (tstate->verbose) {
    printf("index = %g, R0 = %g, RS = %g\n", index, R0, RS);
    printf("k = %g %g %g\n", k[0], k[1], k[2]);
  }
  ELL_3V_COPY(rfRay.dir, refl);
  echoRayColor(rfCol, samp, &rfRay, scene, parm, tstate);
  ELL_3V_COPY(trRay.dir, tran);
  echoRayColor(trCol, samp, &trRay, scene, parm, tstate);
  ELL_3V_SCALEADD(chan, RS, rfCol, 1-RS, trCol);
  chan[0] *= k[0];
  chan[1] *= k[1];
  chan[2] *= k[2];
  chan[3] = 1.0;
  return;
}

void
_echoIntxColorLight(INTXCOLOR_ARGS) {
  
  chan[0] = intx->obj->rgba[0];
  chan[1] = intx->obj->rgba[1];
  chan[2] = intx->obj->rgba[2];
  chan[3] = 1.0;
}

_echoIntxColor_t
_echoIntxColor[ECHO_MATTER_MAX+1] = {
  _echoIntxColorUnknown,
  _echoIntxColorPhong,
  _echoIntxColorGlass,
  _echoIntxColorMetal,
  _echoIntxColorLight,
};
