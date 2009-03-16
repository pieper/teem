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

#define _DECREASE(ell, enn) (           \
  2*((ell) - (enn))            \
  / ( (AIR_ABS(ell) + AIR_ABS(enn))          \
      ? (AIR_ABS(ell) + AIR_ABS(enn))          \
      : 1 )				       \
  )
/*
** this is the core of the worker threads: as long as there are bins
** left to process, get the next one, and process it
*/
int
_pullProcess(pullTask *task) {
  char me[]="_pullProcess", err[BIFF_STRLEN];
  unsigned int binIdx;

  while (task->pctx->binNextIdx < task->pctx->binNum) {
    /* get the index of the next bin to process */
    if (task->pctx->threadNum > 1) {
      airThreadMutexLock(task->pctx->binMutex);
    }
    /* note that we entirely skip bins with no points */
    do {
      binIdx = task->pctx->binNextIdx;
      if (task->pctx->binNextIdx < task->pctx->binNum) {
        task->pctx->binNextIdx++;
      }
    } while (binIdx < task->pctx->binNum
             && 0 == task->pctx->bin[binIdx].pointNum);
    if (task->pctx->threadNum > 1) {
      airThreadMutexUnlock(task->pctx->binMutex);
    }
    if (binIdx == task->pctx->binNum) {
      /* no more bins to process! */
      break;
    }
    if (pullBinProcess(task, binIdx)) {
      sprintf(err, "%s(%u): had trouble on bin %u", me,
              task->threadIdx, binIdx);
      biffAdd(PULL, err); return 1;
    }
  }
  return 0;
}

/* the main loop for each worker thread */
void *
_pullWorker(void *_task) {
  char me[]="_pushWorker", err[BIFF_STRLEN];
  pullTask *task;
  
  task = (pullTask *)_task;

  while (1) {
    if (task->pctx->verbose > 1) {
      printf("%s(%u): waiting on barrier A\n",
             me, task->threadIdx);
    }
    /* pushFinish sets finished prior to the barriers */
    airThreadBarrierWait(task->pctx->iterBarrierA);
    if (task->pctx->finished) {
      if (task->pctx->verbose > 1) {
        printf("%s(%u): done!\n", me, task->threadIdx);
      }
      break;
    }
    /* else there's work to do ... */    
    if (task->pctx->verbose > 1) {
      printf("%s(%u): starting to process\n", me, task->threadIdx);
    }
    if (_pullProcess(task)) {
      /* HEY clearly not threadsafe to have errors ... */
      sprintf(err, "%s: thread %u trouble", me, task->threadIdx);
      biffAdd(PULL, err); 
      task->pctx->finished = AIR_TRUE;
    }
    if (task->pctx->verbose > 1) {
      printf("%s(%u): waiting on barrier B\n",
             me, task->threadIdx);
    }
    airThreadBarrierWait(task->pctx->iterBarrierB);
  }

  return _task;
}

int
pullStart(pullContext *pctx) {
  char me[]="pullStart", err[BIFF_STRLEN];
  unsigned int tidx;

  printf("!%s: hello %p\n", me, pctx);
  pctx->iter = 0; /* have to initialize this here because of seedOnly hack */

  /* the ordering of steps below is important! e.g. gage context has
     to be set up (_pullVolumeSetup) by before its copied (_pullTaskSetup) */
  if (_pullContextCheck(pctx)
      || _pullVolumeSetup(pctx)
      || _pullInfoSetup(pctx) 
      || _pullTaskSetup(pctx)
      || _pullBinSetup(pctx)
      || _pullPointSetup(pctx)) {
    sprintf(err, "%s: trouble setting up context", me);
    biffAdd(PULL, err); return 1;
  }

  if (pctx->threadNum > 1) {
    pctx->binMutex = airThreadMutexNew();
    pctx->iterBarrierA = airThreadBarrierNew(pctx->threadNum);
    pctx->iterBarrierB = airThreadBarrierNew(pctx->threadNum);
    /* start threads 1 and up running; they'll all hit iterBarrierA  */
    for (tidx=1; tidx<pctx->threadNum; tidx++) {
      if (pctx->verbose > 1) {
        printf("%s: spawning thread %d\n", me, tidx);
      }
      airThreadStart(pctx->task[tidx]->thread, _pullWorker,
                     (void *)(pctx->task[tidx]));
    }
  } else {
    pctx->binMutex = NULL;
    pctx->iterBarrierA = NULL;
    pctx->iterBarrierB = NULL;
  }
  printf("!%s: setup for %u threads done\n", me, pctx->threadNum);

  pctx->timeIteration = 0;
  pctx->timeRun = 0;

  return 0;
}

/*
** this is called *after* pullOutputGet
**
** should nix everything created by the many _pull*Setup() functions
*/
int
pullFinish(pullContext *pctx) {
  char me[]="pullFinish", err[BIFF_STRLEN];
  unsigned int tidx;

  if (!pctx) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(PULL, err); return 1;
  }

  pctx->finished = AIR_TRUE;
  if (pctx->threadNum > 1) {
    if (pctx->verbose > 1) {
      printf("%s: finishing workers\n", me);
    }
    airThreadBarrierWait(pctx->iterBarrierA);
    /* worker threads now pass barrierA and see that finished is AIR_TRUE,
       and then bail, so now we collect them */
    for (tidx=pctx->threadNum; tidx>0; tidx--) {
      if (tidx-1) {
        airThreadJoin(pctx->task[tidx-1]->thread,
                      &(pctx->task[tidx-1]->returnPtr));
      }
    }
    pctx->binMutex = airThreadMutexNix(pctx->binMutex);
    pctx->iterBarrierA = airThreadBarrierNix(pctx->iterBarrierA);
    pctx->iterBarrierB = airThreadBarrierNix(pctx->iterBarrierB);
  }

  /* no need for _pullVolumeFinish(pctx), at least not now */
  /* no need for _pullInfoFinish(pctx), at least not now */
  _pullTaskFinish(pctx);
  _pullBinFinish(pctx);
  _pullPointFinish(pctx); /* yes, nixed bins deleted pnts inside, but
                             other buffers still have to be freed */

  return 0;
}
/*
** _pullIterate
**
** (documentation)
**
** NB: this implements the body of thread 0, the master thread
*/
static int
_iterate(pullContext *pctx, int mode) {
  char me[]="_iterate", err[BIFF_STRLEN];
  double time0;
  int myError, E;
  unsigned int ti;

  if (!pctx) {
    sprintf(err, "%s: got NULL pointer", me);
    biffAdd(PULL, err); return 1;
  }
  if (airEnumValCheck(pullProcessMode, mode)) {
    sprintf(err, "%s: process mode %d unrecognized", me, mode);
    biffAdd(PULL, err); return 1;
  }

  /* tell all tasks what mode they're in */
  for (ti=0; ti<pctx->threadNum; ti++) {
    pctx->task[ti]->processMode = mode;
  }
  if (pctx->verbose) {
    printf("%s(%s): iter %d goes w/ %u thrd, %u pnts, enr %g%s\n",
	   me, airEnumStr(pullProcessMode, mode),
	   pctx->iter, pctx->threadNum,
           pullPointNumber(pctx), _pullEnergyTotal(pctx),
           (pctx->permuteOnRebin ? " (por)" : ""));
  }

  time0 = airTime();

  /* the _pullWorker checks finished after iterBarrierA */
  pctx->finished = AIR_FALSE;

  /* initialize index of next bin to be doled out to threads */
  pctx->binNextIdx=0;

  if (pctx->threadNum > 1) {
    airThreadBarrierWait(pctx->iterBarrierA);
  }
  myError = AIR_FALSE;
  if (_pullProcess(pctx->task[0])) {
    sprintf(err, "%s: master thread trouble w/ iter %u", me, pctx->iter);
    biffAdd(PULL, err);
    pctx->finished = AIR_TRUE;
    myError = AIR_TRUE;
  }
  if (pctx->threadNum > 1) {
    airThreadBarrierWait(pctx->iterBarrierB);
  }
  if (pctx->finished) {
    if (!myError) {
      /* we didn't set finished- one of the workers must have */
      sprintf(err, "%s: worker error on iter %u", me, pctx->iter);
      biffAdd(PULL, err); 
    }
    return 1;
  }

  /* depending on mode, run one of the iteration finishers */
  E = 0;
  switch (mode) {
  case pullProcessModeDescent:
    E = _pullIterFinishDescent(pctx); /* includes rebinning */
    break;
  case pullProcessModeNeighLearn:
    /* nothing extra to do */
    break;
  case pullProcessModeAdding:
    E = _pullIterFinishAdding(pctx);
    break;
  case pullProcessModeNixing:
    E = _pullIterFinishNixing(pctx);
    break;
  default:
    sprintf(err, "%s: process mode %d unrecognized", me, mode);
    biffAdd(PULL, err); return 1;
    break;
  }

  pctx->timeIteration = airTime() - time0;

  return 0;
}

int
pullRun(pullContext *pctx) {
  char me[]="pullRun", err[BIFF_STRLEN],
    poutS[AIR_STRLEN_MED];
  Nrrd *npos;
  double time0, time1, enrLast,
    enrNew=AIR_NAN, enrDecrease=AIR_NAN, enrDecreaseAvg=AIR_NAN;
  int converged;
  unsigned firstIter;
  
  if (pctx->verbose) {
    printf("%s: hello\n", me);
  }
  time0 = airTime();
  firstIter = pctx->iter;
  if (pctx->verbose) {
    printf("%s: doing priming iteration (iter now %u)\n", me,
           pctx->iter);
  }
  if (_iterate(pctx, pullProcessModeDescent)) {
    sprintf(err, "%s: trouble on priming iter %u", me, pctx->iter);
    biffAdd(PULL, err); return 1;
  }
  pctx->iter += 1;
  enrLast = enrNew = _pullEnergyTotal(pctx);
  printf("!%s: starting system energy = %g\n", me, enrLast);
  enrDecrease = enrDecreaseAvg = 0;
  converged = AIR_FALSE;
  while ((!pctx->iterMax || pctx->iter < pctx->iterMax) && !converged) {
    if (pctx->snap && !(pctx->iter % pctx->snap)) {
      npos = nrrdNew();
      sprintf(poutS, "snap.%06d.pos.nrrd", pctx->iter);
      if (pullOutputGet(npos, NULL, NULL, NULL, pctx)) {
        sprintf(err, "%s: couldn't get snapshot for iter %d", me, pctx->iter);
        biffAdd(PULL, err); return 1;
      }
      if (nrrdSave(poutS, npos, NULL)) {
        sprintf(err, "%s: couldn't save snapshot for iter %d", me, pctx->iter);
        biffMove(PULL, err, NRRD); return 1;
      }
      npos = nrrdNuke(npos);
    }

    if (_iterate(pctx, pullProcessModeDescent)) {
      sprintf(err, "%s: trouble on iter %d", me, pctx->iter);
      biffAdd(PULL, err); return 1;
    }
    enrNew = _pullEnergyTotal(pctx);
    enrDecrease = _DECREASE(enrLast, enrNew);
    if (firstIter + 1 == pctx->iter) {
      /* we need some way of artificially boosting enrDecreaseAvg when
         we're just starting, so that we thwart the convergence test,
         which we do because we don't have the history of iterations 
         that enrDecreaseAvg is supposed to describe.  Using some scaling
         of enrDecrease is one possible hack. */
      enrDecreaseAvg = 3*enrDecrease;
    } else {
      enrDecreaseAvg = (2*enrDecreaseAvg + enrDecrease)/3;
    }
    if (pctx->verbose) {
      printf("%s: ________ done iter %u: "
             "e=%g,%g, de=%g,%g, s=%g,%g\n",
             me, pctx->iter, enrLast, enrNew, enrDecrease, enrDecreaseAvg,
             _pullStepInterAverage(pctx), _pullStepConstrAverage(pctx));
    }
    if (pctx->popCntlPeriod
	&& (pctx->popCntlPeriod - 1) == (pctx->iter % pctx->popCntlPeriod)
	&& enrDecreaseAvg < pctx->energyDecreasePopCntlMin) {
      if (pctx->verbose) {
	printf("%s: ***** enr decrease %g < %g: trying pop cntl ***** \n",
	       me, enrDecreaseAvg, pctx->energyDecreasePopCntlMin);
      }
      if (_iterate(pctx, pullProcessModeNeighLearn)
          || _iterate(pctx, pullProcessModeAdding)
          || _iterate(pctx, pullProcessModeNixing)) {
	sprintf(err, "%s: trouble with %s for pop cntl on iter %u", me,
		airEnumStr(pullProcessMode, pctx->task[0]->processMode),
                pctx->iter);
	biffAdd(PULL, err); return 1;
      }
    }
    pctx->iter += 1;
    enrLast = enrNew;
    converged = (!pctx->addNum
                 && !pctx->nixNum
                 && AIR_IN_OP(0, enrDecreaseAvg, pctx->energyDecreaseMin));
    if (converged && pctx->verbose) {
      printf("%s: enrDecreaseAvg %g < %g: converged!!\n", me, 
	     enrDecreaseAvg, pctx->energyDecreaseMin);
    }
    _pullPointStepEnergyScale(pctx, pctx->opporStepScale);
    /* call the callback */
    if (pctx->iter_cb) {
      pctx->iter_cb(pctx->data_cb);
    }
  }
  printf("%s: done ((%d|%d)&%d) @iter %u: enr %g, enrDec = %g,%g "
         "%u stuck\n", me,
         !pctx->iterMax, pctx->iter < pctx->iterMax, !converged,
         pctx->iter, enrNew, enrDecrease, enrDecreaseAvg, pctx->stuckNum);
  time1 = airTime();

  pctx->timeRun += time1 - time0;
  pctx->energy = enrNew;

  if (1) {
    /* test inter-particle energy stuff */
    unsigned int szimg=300, ri, si;
    Nrrd *nout;
    pullPoint *pa, *pb;
    double rdir[3], len, r, s, *out, enr, egrad[4];
    airRandMTState *rng;
    rng = pctx->task[0]->rng;
    nout = nrrdNew();
    nrrdMaybeAlloc_va(nout, nrrdTypeDouble, 3, 
                      AIR_CAST(size_t, 3),
                      AIR_CAST(size_t, szimg),
                      AIR_CAST(size_t, szimg));
    out = AIR_CAST(double *, nout->data);
    pa = pullPointNew(pctx);
    pb = pullPointNew(pctx);
    airNormalRand_r(pa->pos + 0, pa->pos + 1, rng);
    airNormalRand_r(pa->pos + 2, pa->pos + 3, rng);
    airNormalRand_r(rdir + 0, rdir + 1, rng);
    airNormalRand_r(rdir + 2, NULL, rng);
    ELL_3V_NORM(rdir, rdir, len);
    for (si=0; si<szimg; si++) {
      s = AIR_AFFINE(0, si, szimg-1,
                     -1.5*pctx->radiusScale, 1.5*pctx->radiusScale);
      for (ri=0; ri<szimg; ri++) {
        r = AIR_AFFINE(0, ri, szimg-1,
                       -1.5*pctx->radiusSpace, 1.5*pctx->radiusSpace);
        ELL_3V_SCALE_ADD2(pb->pos, 1.0, pa->pos, r, rdir);
        pb->pos[3] = pa->pos[3] + s;
        /* now points are in desired test positions */
        enr = _pullEnergyInterParticle(pctx->task[0], pa, pb, 
                                       AIR_ABS(r), AIR_ABS(s), egrad);
        ELL_3V_SET(out + 3*(ri + szimg*si),
                   enr, ELL_3V_DOT(egrad, rdir), egrad[3]);
      }
    }
    nrrdSave("eprobe.nrrd", nout, NULL);
    pullPointNix(pa);
    pullPointNix(pb);
    nrrdNuke(nout);
  }

  return 0;
}
