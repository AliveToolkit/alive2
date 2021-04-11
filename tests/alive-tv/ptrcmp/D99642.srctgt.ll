; RUN: opt -jump-threading -S %s -o - | FileCheck %s

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local i1 @src() local_unnamed_addr {
entry:
  %a = alloca i64, align 8
  store i64 0, i64* %a, align 8
  %i = ptrtoint i64* %a to i64
  %p = bitcast i64* %a to i8*
  %0 = sub i64 0, %i
  %q = getelementptr i8, i8* %p, i64 %0
  %c = icmp eq i8* %q, null
  ; `%a` is non-null at the end of the block, because we store through it.
  ; However, `%q` is derived from `%a` via a GEP that is not `inbounds`, therefore we cannot judge `%q` is non-null as well
  ; and must retain the `icmp` instruction.
  ; CHECK: %c = icmp eq i8* %q, null
  ret i1 %c 
}

define dso_local i1 @tgt() local_unnamed_addr {
if.end:
  ret i1 0
}

; ERROR: Value mismatch
