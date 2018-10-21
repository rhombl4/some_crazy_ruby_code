#!/bin/sh

set -e

make full

cp full tmp.full

#rm -fr out
mkdir -p out

for i in fmodels/FA*_tgt.mdl; do
    for n in `seq 1 40`; do
        out=out/$n
        mkdir -p $out
        b=$(basename $i | sed 's/_tgt\.mdl$//')
        echo $b
        ./tmp.full $i $out/$b.nbt $n &
        #if ! ./tmp.full $i $out/$b.nbt $n; then
        #    echo "$b $n fail"
        #fi
    done
    wait
done
