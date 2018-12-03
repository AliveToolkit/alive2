Alive2
======

WIP; do not use!

Prerequisites
-------------

* Z3 4.8.3
* re2c

Building
--------

```
mkdir build
cd build
cmake ..
make
```

Running
-------
```
cat <<EOT >> test.ll
define i1 @pv(i8 %x, i8 %y) {
  %tmp0 = lshr i8 -1, %y
  %tmp1 = and i8 %tmp0, %x
  %ret = icmp sge i8 %tmp1, %x
  ret i1 %ret
}
EOT
bzip2 test.ll
php ../scripts/run.php test.ll.bz2 instcombine ./alive .../build/llvm/bin/opt
```

Example output:
```
Processing test.ll.alive.opt..

----------------------------------------
Name: pv
  %tmp0 = lshr i8 -1, %y
  %tmp1 = and i8 %tmp0, %x
  %ret = icmp sge i8 %tmp1, %x
  ret i1 %ret
=>
  %tmp0 = lshr i8 -1, %y
  %1 = icmp sge i8 %tmp0, %x
  ret i1 %1

ERROR: Value mismatch

Example:
i8 %y = 0x0 (0)
i8 %x = 0x7f (127)

Source:
i8 %tmp0 = 0xff (255, -1)
i8 %tmp1 = 0x7f (127)
i1 %ret = 0x1 (1, -1)

Target:
i8 %tmp0 = 0xff (255, -1)
i1 %1 = 0x0 (0)
Source value: 0x1 (1, -1)
Target value: 0x0 (0)
```
