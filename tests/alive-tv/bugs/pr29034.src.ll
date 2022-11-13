; https://bugs.llvm.org/show_bug.cgi?id=29034
; FIXME
; To detect this bug,
; 1. infinite loops should be supported
; 2. function calls should be able to update escaped local blocks
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64"

%struct._MUSIC_OP_API_ = type { %struct._FILE_OPERATE_*, %struct.__MUSIC_API* }
%struct._FILE_OPERATE_ = type { %struct._FILE_OPERATE_INIT_*, %struct._lg_dev_info_* }
%struct._FILE_OPERATE_INIT_ = type { i32, i32, i32, i32, ptr, ptr, i32 }
%struct._lg_dev_info_ = type { %struct.os_event, i32, i32, %struct._lg_dev_hdl_*, i8, i8, i8, i8, i8 }
%struct.os_event = type { i8, i32, ptr, %union.anon }
%union.anon = type { %struct.event_cnt }
%struct.event_cnt = type { i16 }
%struct._lg_dev_hdl_ = type { ptr, ptr, ptr, ptr, ptr }
%struct.__MUSIC_API = type <{ ptr, ptr, i32, %struct._DEC_API, %struct._DEC_API_IO*, %struct._FS_BRK_POINT* }>
%struct._DEC_API = type { %struct._DEC_PHY*, ptr, ptr, ptr (ptr)*, ptr (ptr)*, ptr, %struct._AAC_DEFAULT_SETTING, i32, i32, ptr, %struct.decoder_inf*, i32, i8, ptr, i8, ptr }
%struct._DEC_PHY = type { ptr, %struct.__audio_decoder_ops*, ptr, %struct.if_decoder_io, %struct.if_dec_file*, ptr, i32 (ptr)*, i32, i8, %struct.__FF_FR }
%struct.__audio_decoder_ops = type { ptr, i32 (ptr, %struct.if_decoder_io*, ptr)*, i32 (ptr)*, i32 (ptr, i32)*, %struct.decoder_inf* (ptr)*, i32 (ptr)*, i32 (ptr)*, i32 (...)*, i32 (...)*, i32 (...)*, void (ptr, i32)*, void (ptr, i32, ptr, i32)*, i32 (ptr, i32, ptr)* }
%struct.if_decoder_io = type { ptr, i32 (ptr, i32, ptr, i32, i8)*, i32 (ptr, i32, ptr)*, void (ptr, ptr, i32)*, i32 (ptr)*, i32 (ptr, i32, i32)* }
%struct.if_dec_file = type { i32 (ptr, ptr, i32)*, i32 (ptr, i32, i32)* }
%struct.__FF_FR = type { i32, i32, i8, i8, i8 }
%struct._AAC_DEFAULT_SETTING = type { i32, i32, i32 }
%struct.decoder_inf = type { i16, i16, i32, i32 }
%struct._DEC_API_IO = type { ptr, ptr, i16 (ptr, ptr, i16)*, i32 (ptr, i8, i32)*, i32 (%struct.decoder_inf*, i32)*, %struct.__OP_IO, i32, i32 }
%struct.__OP_IO = type { ptr, ptr (ptr, ptr, i32)* }
%struct._FS_BRK_POINT = type { %struct._FS_BRK_INFO, i32, i32 }
%struct._FS_BRK_INFO = type { i32, i32, [8 x i8], i8, i8, i16 }

@.str = external hidden unnamed_addr constant [10 x i8], align 1

; Function Attrs: minsize noreturn nounwind optsize
define void @music_task(ptr nocapture readnone %p) local_unnamed_addr #0 {
entry:
  %mapi = alloca %struct._MUSIC_OP_API_*, align 8
  %0 = bitcast %struct._MUSIC_OP_API_** %mapi to ptr
  call void @llvm.lifetime.start(i64 8, ptr %0) #4
  store %struct._MUSIC_OP_API_* null, %struct._MUSIC_OP_API_** %mapi, align 8
  %call = call i32 @music_decoder_init(%struct._MUSIC_OP_API_** nonnull %mapi) #4
  br label %while.cond

while.cond.loopexit:                              ; preds = %while.cond2
  br label %while.cond

while.cond:                                       ; preds = %while.cond.loopexit, %entry
  %1 = load %struct._MUSIC_OP_API_*, %struct._MUSIC_OP_API_** %mapi, align 8
  %dop_api = getelementptr inbounds %struct._MUSIC_OP_API_, %struct._MUSIC_OP_API_* %1, i64 0, i32 1
  %2 = load %struct.__MUSIC_API*, %struct.__MUSIC_API** %dop_api, align 8
  %file_num = getelementptr inbounds %struct.__MUSIC_API, %struct.__MUSIC_API* %2, i64 0, i32 2
  %3 = bitcast ptr %file_num to ptr
  %call1 = call i32 @music_play_api(%struct._MUSIC_OP_API_* %1, i32 33, i32 0, i32 28, ptr %3) #4
  br label %while.cond2

while.cond2:                                      ; preds = %while.cond2.backedge, %while.cond
  %err.0 = phi i32 [ %call1, %while.cond ], [ %err.0.be, %while.cond2.backedge ]
  switch i32 %err.0, label %sw.default [
    i32 0, label %while.cond.loopexit
    i32 35, label %sw.bb
    i32 11, label %sw.bb7
    i32 12, label %sw.bb13
  ]

sw.bb:                                            ; preds = %while.cond2
  %4 = load %struct._MUSIC_OP_API_*, %struct._MUSIC_OP_API_** %mapi, align 8
  %dop_api4 = getelementptr inbounds %struct._MUSIC_OP_API_, %struct._MUSIC_OP_API_* %4, i64 0, i32 1
  %5 = load %struct.__MUSIC_API*, %struct.__MUSIC_API** %dop_api4, align 8
  %file_num5 = getelementptr inbounds %struct.__MUSIC_API, %struct.__MUSIC_API* %5, i64 0, i32 2
  %6 = load i32, ptr %file_num5, align 1
  %call6 = call i32 (ptr, ...) @printf(ptr getelementptr inbounds ([10 x i8], [10 x i8]* @.str, i64 0, i64 0), i32 %6) #6
  br label %while.cond2.backedge

sw.bb7:                                           ; preds = %while.cond2
  %7 = load %struct._MUSIC_OP_API_*, %struct._MUSIC_OP_API_** %mapi, align 8
  %dop_api8 = getelementptr inbounds %struct._MUSIC_OP_API_, %struct._MUSIC_OP_API_* %7, i64 0, i32 1
  %8 = load %struct.__MUSIC_API*, %struct.__MUSIC_API** %dop_api8, align 8
  %file_num9 = getelementptr inbounds %struct.__MUSIC_API, %struct.__MUSIC_API* %8, i64 0, i32 2
  store i32 1, ptr %file_num9, align 1
  %9 = bitcast ptr %file_num9 to ptr
  %call12 = call i32 @music_play_api(%struct._MUSIC_OP_API_* %7, i32 34, i32 0, i32 24, ptr %9) #4
  br label %while.cond2.backedge

sw.bb13:                                          ; preds = %while.cond2
  %10 = load %struct._MUSIC_OP_API_*, %struct._MUSIC_OP_API_** %mapi, align 8
  %dop_api14 = getelementptr inbounds %struct._MUSIC_OP_API_, %struct._MUSIC_OP_API_* %10, i64 0, i32 1
  %11 = load %struct.__MUSIC_API*, %struct.__MUSIC_API** %dop_api14, align 8
  %file_num15 = getelementptr inbounds %struct.__MUSIC_API, %struct.__MUSIC_API* %11, i64 0, i32 2
  store i32 1, ptr %file_num15, align 1
  %12 = bitcast ptr %file_num15 to ptr
  %call18 = call i32 @music_play_api(%struct._MUSIC_OP_API_* %10, i32 35, i32 0, i32 26, ptr %12) #4
  br label %while.cond2.backedge

sw.default:                                       ; preds = %while.cond2
  %13 = load %struct._MUSIC_OP_API_*, %struct._MUSIC_OP_API_** %mapi, align 8
  %call19 = call i32 @music_play_api(%struct._MUSIC_OP_API_* %13, i32 33, i32 0, i32 22, ptr null) #4
  br label %while.cond2.backedge

while.cond2.backedge:                             ; preds = %sw.default, %sw.bb13, %sw.bb7, %sw.bb
  %err.0.be = phi i32 [ %call19, %sw.default ], [ %call18, %sw.bb13 ], [ %call12, %sw.bb7 ], [ 0, %sw.bb ]
  br label %while.cond2
}

declare void @llvm.lifetime.start(i64, ptr nocapture) #1
declare i32 @music_decoder_init(%struct._MUSIC_OP_API_**) local_unnamed_addr #2
declare i32 @music_play_api(%struct._MUSIC_OP_API_*, i32, i32, i32, ptr) local_unnamed_addr #2
declare i32 @printf(ptr nocapture readonly, ...) local_unnamed_addr #3

attributes #0 = { noreturn nounwind optsize "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-signed-zeros-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { memory(argmem: readwrite) nounwind }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+neon" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+neon" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nounwind }
