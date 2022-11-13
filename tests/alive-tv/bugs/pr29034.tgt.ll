target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64"

%struct._MUSIC_OP_API_ = type {ptr, ptr }
%struct.__MUSIC_API = type <{ ptr, ptr, i32, %struct._DEC_API, ptr, ptr }>
%struct._DEC_API = type { ptr, ptr, ptr, ptr, ptr, ptr, %struct._AAC_DEFAULT_SETTING, i32, i32, ptr, ptr, i32, i8, ptr, i8, ptr }
%struct._AAC_DEFAULT_SETTING = type { i32, i32, i32 }

@.str = external hidden unnamed_addr constant [10 x i8], align 1

define void @music_task(ptr nocapture readnone %p) #0 {
entry:
  %mapi = alloca ptr, align 8
  %0 = bitcast ptr %mapi to ptr
  call void @llvm.lifetime.start(i64 8, ptr %0) #3
  store ptr null, ptr %mapi, align 8
  %call = call i32 @music_decoder_init(ptr nonnull %mapi) #3
  br label %while.cond

while.cond.loopexit:                              ; preds = %while.cond2
  br label %while.cond

while.cond:                                       ; preds = %while.cond.loopexit, %entry
  %1 = load ptr, ptr %mapi, align 8
  %dop_api = getelementptr inbounds %struct._MUSIC_OP_API_, ptr %1, i64 0, i32 1
  %2 = load ptr, ptr %dop_api, align 8
  %file_num = getelementptr inbounds %struct.__MUSIC_API, ptr %2, i64 0, i32 2
  %3 = bitcast ptr %file_num to ptr
  %call1 = call i32 @music_play_api(ptr %1, i32 33, i32 0, i32 28, ptr %3) #3
  br label %while.cond2

while.cond2:                                      ; preds = %while.cond2.backedge, %while.cond
  %err.0 = phi i32 [ %call1, %while.cond ], [ %err.0.be, %while.cond2.backedge ]
  %4 = load ptr, ptr %mapi, align 8
  %dop_api8 = getelementptr inbounds %struct._MUSIC_OP_API_, ptr %4, i64 0, i32 1
  %5 = load ptr, ptr %dop_api8, align 8
  %file_num9 = getelementptr inbounds %struct.__MUSIC_API, ptr %5, i64 0, i32 2
  %6 = bitcast ptr %file_num9 to ptr
  store i32 1, ptr %file_num9, align 1
  switch i32 %err.0, label %sw.default [
    i32 0, label %while.cond.loopexit
    i32 35, label %sw.bb
    i32 11, label %sw.bb7
    i32 12, label %sw.bb13
  ]

sw.bb:                                            ; preds = %while.cond2
  %7 = load i32, ptr %file_num9, align 1
  %call6 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([10 x i8], [10 x i8]* @.str, i64 0, i64 0), i32 %7)
  br label %while.cond2.backedge

sw.bb7:                                           ; preds = %while.cond2
  %call12 = call i32 @music_play_api(ptr %4, i32 34, i32 0, i32 24, ptr %6) #3
  br label %while.cond2.backedge

sw.bb13:                                          ; preds = %while.cond2
  %call18 = call i32 @music_play_api(ptr %4, i32 35, i32 0, i32 26, ptr %6) #3
  br label %while.cond2.backedge

sw.default:                                       ; preds = %while.cond2
  %call19 = call i32 @music_play_api(ptr %4, i32 33, i32 0, i32 22, ptr null) #3
  br label %while.cond2.backedge

while.cond2.backedge:                             ; preds = %sw.default, %sw.bb13, %sw.bb7, %sw.bb
  %err.0.be = phi i32 [ %call19, %sw.default ], [ %call18, %sw.bb13 ], [ %call12, %sw.bb7 ], [ 0, %sw.bb ]
  br label %while.cond2
}

declare void @llvm.lifetime.start(i64, ptr nocapture) #1
declare i32 @music_decoder_init(ptr)
declare i32 @music_play_api(ptr, i32, i32, i32, ptr)
declare i32 @printf(ptr nocapture readonly, ...) #3

attributes #0 = { noreturn nounwind }
attributes #1 = { memory(argmem: readwrite) nounwind }
attributes #3 = { nounwind }
