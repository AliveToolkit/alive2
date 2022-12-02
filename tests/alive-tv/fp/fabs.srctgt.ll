define void @src() {
  %one = fdiv float 1.0, 1.0
  %minusone = fdiv float 1.0, -1.0
  %zero = fdiv float 0.0, 1.0
  %minuszero = fdiv float 0.0, -1.0
  %inf = fdiv float 1.0, 0.0
  %neginf = fdiv float 1.0, -0.0
  %nan = fdiv float 0.0, 0.0
  %negnan = fdiv float 0.0, -0.0

  %f1 = call float @llvm.fabs.f32(float %one)
  %f2 = call float @llvm.fabs.f32(float %minusone)
  %f3 = call float @llvm.fabs.f32(float %zero)
  %f4 = call float @llvm.fabs.f32(float %minuszero)
  %f5 = call float @llvm.fabs.f32(float %inf)
  %f6 = call float @llvm.fabs.f32(float %neginf)
  %f7 = call float @llvm.fabs.f32(float %nan)
  %f8 = call float @llvm.fabs.f32(float %negnan)

  call void @f(float %f1, float %f2, float %f3, float %f4, float %f5, float %f6, float %f7, float %f8)
  ret void
}

define void @tgt() {
  %one = fdiv float 1.0, 1.0
  %zero = fdiv float 0.0, 1.0
  %inf = fdiv float 1.0, 0.0
  %nan = fdiv float 0.0, 0.0

  call void @f(float %one, float %one, float %zero, float %zero, float %inf, float %inf, float %nan, float %nan)
  ret void
}

declare void @f(float, float, float, float, float, float, float, float)
declare float @llvm.fabs.f32(float)
