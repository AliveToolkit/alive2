; https://bugs.llvm.org/show_bug.cgi?id=23599
; TODO: To reproduce this, bytes of escaped local blocks should be checked

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ether_addr = type { [6 x i8] }
%struct.ether_header = type { [6 x i8], [6 x i8] }

define void @src(ptr captures(none) readonly %ether_src, ptr captures(none) readonly %ether_dst) {
entry:
  %eth = alloca %struct.ether_header, align 1
  %0 = getelementptr inbounds %struct.ether_header, ptr %eth, i64 0, i32 0, i64 0
  call void @llvm.lifetime.start(i64 12, ptr %0) #1
  call void @llvm.memset.p0i8.i64(ptr %0, i8 0, i64 12, i32 1, i1 false)
  %arraydecay = getelementptr inbounds %struct.ether_header, ptr %eth, i64 0, i32 1, i64 0
  %1 = getelementptr inbounds %struct.ether_addr, ptr %ether_src, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(ptr %arraydecay, ptr %1, i64 6, i32 1, i1 false)
  %2 = getelementptr inbounds %struct.ether_addr, ptr %ether_dst, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(ptr %0, ptr %2, i64 6, i32 1, i1 false)
  call void @_Z5PrintRK12ether_header(ptr dereferenceable(12) %eth)
  call void @llvm.lifetime.end(i64 12, ptr %0) #1
  ret void
}

define void @tgt(ptr captures(none) readonly %ether_src, ptr captures(none) readonly %ether_dst) {
entry:
  %eth = alloca %struct.ether_header, align 1
  %0 = getelementptr inbounds %struct.ether_header, ptr %eth, i64 0, i32 0, i64 0
  call void @llvm.lifetime.start(i64 12, ptr %0) #1
  %arraydecay = getelementptr inbounds %struct.ether_header, ptr %eth, i64 0, i32 1, i64 0
  %1 = getelementptr inbounds %struct.ether_addr, ptr %ether_src, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(ptr %arraydecay, ptr %1, i64 6, i32 1, i1 false)
  %2 = getelementptr inbounds %struct.ether_addr, ptr %ether_dst, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(ptr %0, ptr %2, i64 6, i32 1, i1 false)
  %3 = getelementptr i8, ptr %0, i64 6
  call void @llvm.memset.p0i8.i64(ptr %3, i8 0, i64 6, i32 1, i1 false)
  call void @_Z5PrintRK12ether_header(ptr dereferenceable(12) %eth)
  call void @llvm.lifetime.end(i64 12, ptr %0) #1
  ret void
}


declare void @llvm.lifetime.start(i64, ptr captures(none))
declare void @llvm.memset.p0i8.i64(ptr captures(none), i8, i64, i32, i1)
declare void @llvm.memcpy.p0i8.p0i8.i64(ptr captures(none), ptr captures(none) readonly, i64, i32, i1)
declare void @_Z5PrintRK12ether_header(ptr dereferenceable(12))
declare void @llvm.lifetime.end(i64, ptr captures(none))
