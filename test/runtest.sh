#!/bin/sh



runone() {
  ops=$1
  b=$2
  t=$3
  exe=./addr2line
  $exe  $ops >$t
  if [ $? -ne 0 ]
  then
    echo "FAILED execution running $exe $ops"
    exit 1
  fi
  diff $b $t
  if [ $? -ne 0 ]
  then
    echo "FAILED comparison running  $exe $ops"
    echo "If new is correct, mv $t $b"
    exit 1
  fi
}

runone "-a -e test/test   0x1040"    test/base1 junk.test1
runone "-a -e test/test   0x1040"    test/base2 junk.test2
runone "-a -b -e test/test   0x1040" test/base3 junk.test3
runone "-a -n -e test/test   0x1040" test/base4 junk.test4
runone "-a -e test/dwarf5test  0x1041 0x1046"    test/base5 junk.test5
runone "-a -b -e test/dwarf5test  0x1041 0x1046" test/base6 junk.test6
runone "-a -n -e test/dwarf5test  0x1041 0x1046" test/base7 junk.test7
echo "PASS addr2line tests"

