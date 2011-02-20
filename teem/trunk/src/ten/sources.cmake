# This variable will help provide a master list of all the sources.
# Add new source files here.
SET(TEN_SOURCES
  aniso.c
  bimod.c
  bvec.c
  chan.c
  defaultsTen.c
  enumsTen.c
  epireg.c
  estimate.c
  fiber.c
  fiberMethods.c
  glyph.c
  grads.c
  miscTen.c
  mod.c
  path.c
  privateTen.h
  qglox.c
  qseg.c
  ten.h
  tenDwiGage.c
  tenGage.c
  tenMacros.h
  tendAbout.c
  tendAnhist.c
  tendAnplot.c
  tendAnscale.c
  tendAnvol.c
  tendAvg.c
  tendBfit.c
  tendBmat.c
  tendEllipse.c
  tendEpireg.c
  tendEstim.c
  tendEval.c
  tendEvaladd.c
  tendEvalmult.c
  tendEvalclamp.c
  tendEvalpow.c
  tendEvec.c
  tendEvecrgb.c
  tendEvq.c
  tendExp.c
  tendExpand.c
  tendFiber.c
  tendFlotsam.c
  tendGlyph.c
  tendGrads.c
  tendHelix.c
  tendLog.c
  tendMake.c
  tendNorm.c
  tendPoint.c
  tendSatin.c
  tendShrink.c
  tendSim.c
  tendMsim.c
  tendMfit.c
  tendMconv.c
  tendSlice.c
  tendSten.c
  tendTconv.c
  tendTriple.c
  tendUnmf.c
  tensor.c
  triple.c
  experSpec.c
  tenModel.c
  modelZero.c
  modelB0.c
  modelBall.c
  model1Stick.c
  modelBall1StickEMD.c
  modelBall1Stick.c
  modelBall1Cylinder.c
  model1Cylinder.c
  model1Tensor2.c
  )

ADD_TEEM_LIBRARY(ten ${TEN_SOURCES})
