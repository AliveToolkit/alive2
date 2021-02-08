; TEST-ARGS: -src-unroll=1 -tgt-unroll=1

define void @fn1() {
entry:
  br i1 undef, label %if.then, label %thirdphiblock

if.then:
  br i1 false, label %firstphiblock, label %temp

firstphiblock:
  br i1 undef, label %thirdphiblock, label %secondphiblock

secondphiblock:
  br i1 undef, label %firstphiblock, label %thirdphiblock

thirdphiblock:
  br label %secondphiblock

temp:
  ret void
}
