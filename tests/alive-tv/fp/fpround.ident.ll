define void @src() {
  %a = call float @llvm.ceil.f32(float 0.5)
  %b = call float @llvm.ceil.f32(float -0.5)
  %c = call float @llvm.floor.f32(float 0.5)
  %d = call float @llvm.floor.f32(float -0.5)
  %e = call float @llvm.round.f32(float 0.5)
  %f = call float @llvm.round.f32(float -0.5)
  %g = call float @llvm.trunc.f32(float 0.5)
  %h = call float @llvm.trunc.f32(float -0.5)
  %i = call float @llvm.roundeven.f32(float 0.5)
  %j = call float @llvm.roundeven.f32(float -0.5)
  call void (...) @test(float %a, float %b, float %c, float %d, float %e,
                        float %f, float %g, float %h, float %i, float %j)
  ret void
}

declare float @llvm.ceil.f32(float) nounwind memory(none)
declare float @llvm.floor.f32(float) nounwind memory(none)
declare float @llvm.round.f32(float) nounwind memory(none)
declare float @llvm.roundeven.f32(float) nounwind memory(none)
declare float @llvm.trunc.f32(float) nounwind memory(none)

declare void @test(...)
