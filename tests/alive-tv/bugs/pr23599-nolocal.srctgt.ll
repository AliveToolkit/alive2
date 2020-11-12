; https://bugs.llvm.org/show_bug.cgi?id=23599

%struct.ether_addr = type { [6 x i8] }
%struct.ether_header = type { [6 x i8], [6 x i8] }

@glb = global %struct.ether_header undef

define void @src(%struct.ether_addr* nocapture readonly %ether_src, %struct.ether_addr* nocapture readonly %ether_dst) {
entry:
  %eth = alloca %struct.ether_header, align 1
  %0 = getelementptr inbounds %struct.ether_header, %struct.ether_header* %eth, i64 0, i32 0, i64 0
  call void @llvm.lifetime.start(i64 12, i8* %0)
  call void @llvm.memset.p0i8.i64(i8* %0, i8 0, i64 12, i32 1, i1 false)
  %arraydecay = getelementptr inbounds %struct.ether_header, %struct.ether_header* %eth, i64 0, i32 1, i64 0
  %1 = getelementptr inbounds %struct.ether_addr, %struct.ether_addr* %ether_src, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arraydecay, i8* %1, i64 6, i32 1, i1 false)
  %2 = getelementptr inbounds %struct.ether_addr, %struct.ether_addr* %ether_dst, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* %2, i64 6, i32 1, i1 false)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* bitcast (%struct.ether_header* @glb to i8*), i8* %0, i64 12, i32 1, i1 false)
  call void @llvm.lifetime.end(i64 12, i8* %0)
  ret void
}

define void @tgt(%struct.ether_addr* nocapture readonly %ether_src, %struct.ether_addr* nocapture readonly %ether_dst) {
entry:
  %eth = alloca %struct.ether_header, align 1
  %0 = getelementptr inbounds %struct.ether_header, %struct.ether_header* %eth, i64 0, i32 0, i64 0
  call void @llvm.lifetime.start(i64 12, i8* %0)
  %arraydecay = getelementptr inbounds %struct.ether_header, %struct.ether_header* %eth, i64 0, i32 1, i64 0
  %1 = getelementptr inbounds %struct.ether_addr, %struct.ether_addr* %ether_src, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arraydecay, i8* %1, i64 6, i32 1, i1 false)
  %2 = getelementptr inbounds %struct.ether_addr, %struct.ether_addr* %ether_dst, i64 0, i32 0, i64 0
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* %2, i64 6, i32 1, i1 false)
  %3 = getelementptr i8, i8* %0, i64 6
  call void @llvm.memset.p0i8.i64(i8* %3, i8 0, i64 6, i32 1, i1 false)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* bitcast (%struct.ether_header* @glb to i8*), i8* %0, i64 12, i32 1, i1 false)
  call void @llvm.lifetime.end(i64 12, i8* %0)
  ret void
}


declare void @llvm.lifetime.start(i64, i8* nocapture)

declare void @llvm.memset.p0i8.i64(i8* nocapture, i8, i64, i32, i1)

declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture readonly, i64, i32, i1)

declare void @_Z5PrintRK12ether_header(%struct.ether_header* dereferenceable(12))

declare void @llvm.lifetime.end(i64, i8* nocapture)

; ERROR: Mismatch in memory
