/*
  teem: Gordon Kindlmann's research software
  Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998 University of Utah

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

#include "gage.h"
#include "privateGage.h"

gageItemEntry
_gageVecTable[GAGE_VEC_ITEM_MAX+1] = {
  /* enum value        len,deriv,  prereqs,                                             parent item, index*/
  {gageVecVector,        3,  0,  {-1, -1, -1, -1, -1},                                           -1,  -1},
  {gageVecLength,        1,  0,  {gageVecVector, -1, -1, -1, -1},                                -1,  -1},
  {gageVecNormalized,    3,  0,  {gageVecVector, gageVecLength, -1, -1, -1},                     -1,  -1},
  {gageVecJacobian,      9,  1,  {-1, -1, -1, -1, -1},                                           -1,  -1},
  {gageVecDivergence,    1,  1,  {gageVecJacobian, -1, -1, -1, -1},                              -1,  -1},
  {gageVecCurl,          3,  1,  {gageVecJacobian, -1, -1, -1, -1},                              -1,  -1},
  {gageVecHelicity,      1,  1,  {gageVecVector, gageVecCurl, -1, -1, -1},                       -1,  -1},
  {gageVecNormHelicity,  1,  1,  {gageVecNormalized, gageVecCurl, -1, -1, -1},                   -1,  -1},
  {gageVecLambda2,       1,  1,  {gageVecJacobian, -1, -1, -1, -1},                              -1,  -1},
  {gageVecHessian,      27,  2,  {-1, -1, -1, -1, -1},                                           -1,  -1},
  {gageVecCurlGradient,  9,  2,  {gageVecHessian, -1, -1, -1, -1},                               -1,  -1},
  {gageVecHelGradient,   3,  2,  {gageVecVector, gageVecJacobian, gageVecCurl, 
				  gageVecCurlGradient, -1},                                      -1,  -1},
  {gageVecDirHelDeriv,   1,  2,  {gageVecNormalized, gageVecHelGradient, -1, -1, -1},            -1,  -1},
  {gageVecProjHelGradient, 3,2,  {gageVecNormalized, gageVecHelGradient, gageVecDirHelDeriv,
				  -1, -1},                                                       -1,  -1},
  /* HEY: these should change to sub-items!!! */
  {gageVecGradient0,     3,  1,  {gageVecJacobian, -1, -1, -1, -1},                              -1,  -1},
  {gageVecGradient1,     3,  1,  {gageVecJacobian, -1, -1, -1, -1},                              -1,  -1},
  {gageVecGradient2,     3,  1,  {gageVecJacobian, -1, -1, -1, -1},                              -1,  -1},
  {gageVecMultiGrad,     9,  1,  {gageVecGradient0, gageVecGradient1, gageVecGradient2, -1, -1}, -1,  -1},
  {gageVecMGFrob,        1,  1,  {gageVecMultiGrad, -1, -1, -1, -1},                             -1,  -1},
  {gageVecMGEval,        3,  1,  {gageVecMultiGrad, -1, -1, -1, -1},                             -1,  -1},
  {gageVecMGEvec,        9,  1,  {gageVecMultiGrad, gageVecMGEval, -1, -1, -1},                  -1,  -1}
};

void
_gageVecFilter (gageContext *ctx, gagePerVolume *pvl) {
  char me[]="_gageVecFilter";
  gage_t *fw00, *fw11, *fw22, *vec, *jac, *hes;
  int fd;

  fd = GAGE_FD(ctx);
  vec = pvl->directAnswer[gageVecVector];
  jac = pvl->directAnswer[gageVecJacobian];
  hes = pvl->directAnswer[gageVecHessian];
  if (!ctx->parm.k3pack) {
    fprintf(stderr, "!%s: sorry, 6pack filtering not implemented\n", me);
    return;
  }
  fw00 = ctx->fw + fd*3*gageKernel00;
  fw11 = ctx->fw + fd*3*gageKernel11;
  fw22 = ctx->fw + fd*3*gageKernel22;
  /* perform the filtering */
  switch (fd) {
  case 2:
#define DOIT_2(J) \
      gageScl3PFilter2(pvl->iv3 + J*8, pvl->iv2 + J*4, pvl->iv1 + J*2, \
		       fw00, fw11, fw22, \
                       vec + J, jac + J*3, hes + J*9, \
		       pvl->needD[0], pvl->needD[1], pvl->needD[2])
      /* 2nd order derivative computation with this scheme is no good idea */
    DOIT_2(0); DOIT_2(1); DOIT_2(2); 
    break;
  case 4:
#define DOIT_4(J) \
      gageScl3PFilter4(pvl->iv3 + J*64, pvl->iv2 + J*16, pvl->iv1 + J*4, \
		       fw00, fw11, fw22, \
                       vec + J, jac + J*3, hes + J*9, \
		       pvl->needD[0], pvl->needD[1], pvl->needD[2])
    DOIT_4(0); DOIT_4(1); DOIT_4(2); 
    break;
  default:
#define DOIT_N(J)\
      gageScl3PFilterN(fd, \
                       pvl->iv3 + J*fd*fd*fd, \
                       pvl->iv2 + J*fd*fd, pvl->iv1 + J*fd, \
		       fw00, fw11, fw22, \
                       vec + J, jac + J*3, hes + J*9, \
		       pvl->needD[0], pvl->needD[1], pvl->needD[2])
    DOIT_N(0); DOIT_N(1); DOIT_N(2); 
    break;
  }

  return;
}

void
_gageVecAnswer (gageContext *ctx, gagePerVolume *pvl) {
  char me[]="_gageVecAnswer";
  double cmag, tmpMat[9], mgevec[9], mgeval[3];
  double symm[9], asym[9], tran[9], eval[3];
  gage_t *vecAns, *normAns, *jacAns, *curlAns, *hesAns, *curlGradAns, 
         *helGradAns, *dirHelDirAns;
  int asw;

  vecAns       = pvl->directAnswer[gageVecVector];
  normAns      = pvl->directAnswer[gageVecNormalized];
  jacAns       = pvl->directAnswer[gageVecJacobian];
  curlAns      = pvl->directAnswer[gageVecCurl];
  hesAns       = pvl->directAnswer[gageVecHessian];
  curlGradAns  = pvl->directAnswer[gageVecCurlGradient];
  helGradAns   = pvl->directAnswer[gageVecHelGradient];
  dirHelDirAns = pvl->directAnswer[gageVecDirHelDeriv];

  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecVector)) {
    /* done if doV */
    if (ctx->verbose) {
      fprintf(stderr, "vec = ");
      ell_3v_PRINT(stderr, vecAns);
    }
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecLength)) {
    pvl->directAnswer[gageVecLength][0] = ELL_3V_LEN(vecAns);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecNormalized)) {
    if (pvl->directAnswer[gageVecLength][0]) {
      ELL_3V_SCALE(normAns, 1.0/pvl->directAnswer[gageVecLength][0], vecAns);
    } else {
      ELL_3V_COPY(normAns, gageZeroNormal);
    }
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecJacobian)) {
    /* done if doD1 */
    /*
      0:dv_x/dx  1:dv_x/dy  2:dv_x/dz
      3:dv_y/dx  4:dv_y/dy  5:dv_y/dz
      6:dv_z/dx  7:dv_z/dy  8:dv_z/dz
    */
    if (ctx->verbose) {
      fprintf(stderr, "%s: jac = \n", me);
      ell_3m_PRINT(stderr, jacAns);
    }
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecDivergence)) {
    pvl->directAnswer[gageVecDivergence][0] = jacAns[0] + jacAns[4] + jacAns[8];
    if (ctx->verbose) {
      fprintf(stderr, "%s: div = %g + %g + %g  = %g\n", me,
	      jacAns[0], jacAns[4], jacAns[8],
	      pvl->directAnswer[gageVecDivergence][0]);
    }
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecCurl)) {
    ELL_3V_SET(curlAns,
	       jacAns[7] - jacAns[5],
	       jacAns[2] - jacAns[6],
	       jacAns[3] - jacAns[1]);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecHelicity)) {
    pvl->directAnswer[gageVecHelicity][0] = 
	ELL_3V_DOT(vecAns, curlAns);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecNormHelicity)) {
    cmag = ELL_3V_LEN(curlAns);
    pvl->directAnswer[gageVecNormHelicity][0] = 
	cmag ? ELL_3V_DOT(normAns, curlAns)/cmag : 0;
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecLambda2)) {
      ELL_3M_TRANSPOSE(tran, jacAns);
      // symmetric part
      ELL_3M_SCALE_ADD2(symm, 0.5, jacAns,  0.5, tran);
      // antisymmetric part
      ELL_3M_SCALE_ADD2(asym, 0.5, jacAns, -0.5, tran);
      // square symmetric part
      ELL_3M_MUL(tmpMat, symm, symm);
      ELL_3M_COPY(symm, tmpMat);
      // square antisymmetric part
      ELL_3M_MUL(tmpMat, asym, asym);
      // sum of both
      ELL_3M_ADD2(symm, symm, tmpMat);
      asw = ell_3m_eigenvalues_d(eval, symm, AIR_TRUE);
      if (asw == ell_cubic_root_triple)
	  pvl->directAnswer[gageVecLambda2][0] = eval[0];
      else if (asw == ell_cubic_root_single_double)
	  pvl->directAnswer[gageVecLambda2][0] = eval[1];
      else if (asw == ell_cubic_root_three)
      {
	  if (eval[0]<=eval[1])
	      if (eval[1]<=eval[2])
		  pvl->directAnswer[gageVecLambda2][0] = eval[1];
	      else 
		  if (eval[0]<=eval[2])
		      pvl->directAnswer[gageVecLambda2][0] = eval[2];
		  else
		      pvl->directAnswer[gageVecLambda2][0] = eval[0];
	  else
	      if (eval[2]<=eval[1])
		  pvl->directAnswer[gageVecLambda2][0] = eval[1];
	      else
		  if (eval[0]<=eval[2])
		      pvl->directAnswer[gageVecLambda2][0] = eval[0];
		  else
		      pvl->directAnswer[gageVecLambda2][0] = eval[2];
      }
  }
  /* 2nd order vector derivative continued */ 
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecHessian)) {
      /* done if doD2 */
      /* the ordering is induced by the scalar hessian computation :
	 0:d2v_x/dxdx   1:d2v_x/dxdy   2:d2v_x/dxdz
	 3:d2v_x/dydx   4:d2v_x/dydy   5:d2v_x/dydz
	 6:d2v_x/dzdx   7:d2v_x/dzdy   8:d2v_x/dzdz
	 9:d2v_y/dxdx       [...]
	     [...]
	24:dv2_z/dzdx  25:d2v_z/dzdy  26:d2v_z/dzdz
      */
    if (ctx->verbose) {
      fprintf(stderr, "%s: hes = \n", me);
      ell_3m_PRINT(stderr, hesAns); /* ?? */
    }
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecCurlGradient)) {
      pvl->directAnswer[gageVecCurlGradient][0] = hesAns[21]-hesAns[15];
      pvl->directAnswer[gageVecCurlGradient][1] = hesAns[22]-hesAns[16];
      pvl->directAnswer[gageVecCurlGradient][2] = hesAns[23]-hesAns[17];
      pvl->directAnswer[gageVecCurlGradient][3] = hesAns[ 6]-hesAns[18];
      pvl->directAnswer[gageVecCurlGradient][4] = hesAns[ 7]-hesAns[19];
      pvl->directAnswer[gageVecCurlGradient][5] = hesAns[ 8]-hesAns[20];
      pvl->directAnswer[gageVecCurlGradient][6] = hesAns[ 9]-hesAns[ 1];
      pvl->directAnswer[gageVecCurlGradient][7] = hesAns[10]-hesAns[ 2];
      pvl->directAnswer[gageVecCurlGradient][8] = hesAns[11]-hesAns[ 3];
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecHelGradient)) {
      pvl->directAnswer[gageVecHelGradient][0] = 
	  jacAns[0]*curlAns[0]+
	  jacAns[3]*curlAns[1]+
	  jacAns[6]*curlAns[2]+
	  curlGradAns[0]*vecAns[0]+
	  curlGradAns[3]*vecAns[1]+
	  curlGradAns[6]*vecAns[2];
      pvl->directAnswer[gageVecHelGradient][1] = 
	  jacAns[1]*curlAns[0]+
	  jacAns[4]*curlAns[1]+
	  jacAns[7]*curlAns[2]+
	  curlGradAns[1]*vecAns[0]+
	  curlGradAns[4]*vecAns[1]+
	  curlGradAns[7]*vecAns[2];
      pvl->directAnswer[gageVecHelGradient][0] = 
	  jacAns[2]*curlAns[0]+
	  jacAns[5]*curlAns[1]+
	  jacAns[8]*curlAns[2]+
	  curlGradAns[2]*vecAns[0]+
	  curlGradAns[5]*vecAns[1]+
	  curlGradAns[8]*vecAns[2];
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecDirHelDeriv)) {
      pvl->directAnswer[gageVecDirHelDeriv][0] = 
	ELL_3V_DOT(normAns, helGradAns);	  
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecProjHelGradient)) {
      pvl->directAnswer[gageVecDirHelDeriv][0] = 
	  helGradAns[0]-dirHelDirAns[0]*normAns[0];
      pvl->directAnswer[gageVecDirHelDeriv][1] = 
	  helGradAns[1]-dirHelDirAns[0]*normAns[1];
      pvl->directAnswer[gageVecDirHelDeriv][2] = 
	  helGradAns[2]-dirHelDirAns[0]*normAns[2];
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecGradient0)) {
    ELL_3V_SET(pvl->directAnswer[gageVecGradient0],
	       jacAns[0],
	       jacAns[1],
	       jacAns[2]);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecGradient1)) {
    ELL_3V_SET(pvl->directAnswer[gageVecGradient1],
	       jacAns[3],
	       jacAns[4],
	       jacAns[5]);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecGradient2)) {
    ELL_3V_SET(pvl->directAnswer[gageVecGradient2],
	       jacAns[6],
	       jacAns[7],
	       jacAns[8]);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecMultiGrad)) {
    ELL_3M_IDENTITY_SET(pvl->directAnswer[gageVecMultiGrad]);
    ELL_3MV_OUTER_ADD(pvl->directAnswer[gageVecMultiGrad],
		      pvl->directAnswer[gageVecGradient0],
		      pvl->directAnswer[gageVecGradient0]);
    ELL_3MV_OUTER_ADD(pvl->directAnswer[gageVecMultiGrad],
		      pvl->directAnswer[gageVecGradient1],
		      pvl->directAnswer[gageVecGradient1]);
    ELL_3MV_OUTER_ADD(pvl->directAnswer[gageVecMultiGrad],
		      pvl->directAnswer[gageVecGradient2],
		      pvl->directAnswer[gageVecGradient2]);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecMGFrob)) {
    pvl->directAnswer[gageVecMGFrob][0] 
      = ELL_3M_FROB(pvl->directAnswer[gageVecMultiGrad]);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecMGEval)) {
    ELL_3M_COPY(tmpMat, pvl->directAnswer[gageVecMultiGrad]);
    /* HEY: look at the return value for root multiplicity? */
    ell_3m_eigensolve_d(mgeval, mgevec, tmpMat, AIR_TRUE);
    ELL_3V_COPY(pvl->directAnswer[gageVecMGEval], mgeval);
  }
  if (GAGE_QUERY_ITEM_TEST(pvl->query, gageVecMGEvec)) {
    ELL_3M_COPY(pvl->directAnswer[gageVecMGEvec], mgevec);
  }

  return;
}

char
_gageVecStr[][AIR_STRLEN_SMALL] = {
  "(unknown gageVec)",
  "vector",
  "length",
  "normalized",
  "Jacobian",
  "divergence",
  "curl",
  "helicity",
  "normalized helicity",
  "lambda2",
  "vector hessian",
  "curl gradient",
  "helicity gradient",
  "directional helicity derivative",
  "projected helicity gradient",
  "gradient0",
  "gradient1",
  "gradient2",
  "multigrad",
  "frob(multigrad)",
  "multigrad eigenvalues",
  "multigrad eigenvectors",
};

char
_gageVecDesc[][AIR_STRLEN_MED] = {
  "unknown gageVec query",
  "component-wise-interpolated vector",
  "length of vector",
  "normalized vector",
  "3x3 Jacobian",
  "divergence",
  "curl",
  "helicity: dot(vector,curl)",
  "normalized helicity",
  "lambda2 value for vortex characterization",
  "3x3x3 second-order vector derivative",
  "3x3 derivative of curl",
  "gradient of helicity",
  "directional derivative of helicity along flow",
  "projection of the helicity gradient onto plane orthogonal to flow",
  "gradient of 1st component of vector",
  "gradient of 2nd component of vector",
  "gradient of 3rd component of vector",
  "multi-gradient: sum of outer products of gradients",
  "frob norm of multi-gradient",
  "eigenvalues of multi-gradient",
  "eigenvectors of multi-gradient"
};

int
_gageVecVal[] = {
  gageVecUnknown,
  gageVecVector,
  gageVecLength,
  gageVecNormalized,
  gageVecJacobian,
  gageVecDivergence,
  gageVecCurl,
  gageVecHelicity,
  gageVecNormHelicity,
  gageVecLambda2,
  gageVecHessian,
  gageVecCurlGradient,
  gageVecHelGradient,
  gageVecDirHelDeriv,
  gageVecProjHelGradient,
  gageVecGradient0,
  gageVecGradient1,
  gageVecGradient2,
  gageVecMultiGrad,
  gageVecMGFrob,
  gageVecMGEval,
  gageVecMGEvec,
};

#define GV_V gageVecVector
#define GV_L gageVecLength
#define GV_N gageVecNormalized
#define GV_J gageVecJacobian
#define GV_D gageVecDivergence
#define GV_C gageVecCurl
#define GV_H  gageVecHelicity
#define GV_NH gageVecNormHelicity
#define GV_LB gageVecLambda2
#define GV_VH gageVecHessian
#define GV_CG gageVecCurlGradient
#define GV_HG gageVecHelGradient
#define GV_DH gageVecDirHelDeriv
#define GV_PH gageVecProjHelGradient
#define GV_G0 gageVecGradient0
#define GV_G1 gageVecGradient1
#define GV_G2 gageVecGradient2
#define GV_MG gageVecMultiGrad
#define GV_MF gageVecMGFrob
#define GV_ML gageVecMGEval
#define GV_MC gageVecMGEvec

char
_gageVecStrEqv[][AIR_STRLEN_SMALL] = {
  "v", "vector", "vec",
  "l", "length", "len",
  "n", "normalized", "normalized vector",
  "jacobian", "jac", "j",
  "divergence", "div", "d",
  "curl", "c",
  "h", "hel", "hell",
  "nh", "nhel", "normhel", "normhell",
  "lbda2", "lambda2",
  "vh", "vhes", "vhessian", "vector hessian",
  "cg", "curlgrad", "curlg", "curljac", "curl gradient",
  "hg", "helg", "helgrad", "helicity gradient",
  "dirhelderiv", "dhd", "ddh", "directional helicity derivative",
  "phg", "projhel", "projhelgrad", "projected helicity gradient",
  "g0", "grad0", "gradient0",
  "g1", "grad1", "gradient1",
  "g2", "grad2", "gradient2",
  "mg", "multigrad",
  "mgfrob",
  "mgeval", "mg eval", "multigrad eigenvalues",
  "mgevec", "mg evec", "multigrad eigenvectors",
  ""
};

int
_gageVecValEqv[] = {
  GV_V, GV_V, GV_V,
  GV_L, GV_L, GV_L,
  GV_N, GV_N, GV_N,
  GV_J, GV_J, GV_J,
  GV_D, GV_D, GV_D,
  GV_C, GV_C,
  GV_H, GV_H, GV_H,
  GV_NH, GV_NH, GV_NH, GV_NH,
  GV_LB, GV_LB,
  GV_VH, GV_VH, GV_VH, GV_VH,
  GV_CG, GV_CG, GV_CG, GV_CG, GV_CG,
  GV_HG, GV_HG, GV_HG, GV_HG,
  GV_DH, GV_DH, GV_DH, GV_DH, 
  GV_PH, GV_PH, GV_PH, GV_PH, 
  GV_G0, GV_G0, GV_G0,
  GV_G1, GV_G1, GV_G1,
  GV_G2, GV_G2, GV_G2,
  GV_MG, GV_MG,
  GV_MF,
  GV_ML, GV_ML, GV_ML,
  GV_MC, GV_MC, GV_MC
};

airEnum
_gageVec = {
  "gageVec",
  GAGE_VEC_ITEM_MAX+1,
  _gageVecStr, _gageVecVal,
  _gageVecDesc,
  _gageVecStrEqv, _gageVecValEqv,
  AIR_FALSE
};
airEnum *
gageVec = &_gageVec;

gageKind
_gageKindVec = {
  "vector",
  &_gageVec,
  1,
  3,
  GAGE_VEC_ITEM_MAX,
  _gageVecTable,
  _gageVecIv3Print,
  _gageVecFilter,
  _gageVecAnswer
};
gageKind *
gageKindVec = &_gageKindVec;

