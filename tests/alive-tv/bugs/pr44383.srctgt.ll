; Found by Alive2

define <3 x i1> @src() {
  %x = call <3 x i8> @gen3x8()
  %tmp0 = and <3 x i8> %x, <i8 3, i8 undef, i8 3>
  %ret = icmp sgt <3 x i8> %x, %tmp0
  ret <3 x i1> %ret
}

define <3 x i1> @tgt() {

  %x = call <3 x i8> @gen3x8()
  %1 = icmp sgt <3 x i8> %x, <i8 3, i8 undef, i8 3>
  ret <3 x i1> %1
}

declare <3 x i8> @gen3x8()

; ERROR: Target's return value is more undefined
