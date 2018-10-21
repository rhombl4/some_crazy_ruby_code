#!/bin/sh

set -e

make

cp light tmp.light

rm -fr out
mkdir -p out

for i in models/*.mdl; do
    for n in `seq 1 20`; do
        out=out/$n
        mkdir -p $out
        b=$(basename $i | sed 's/_tgt\.mdl$//')
        echo $b
        #./light $i $out/$b.nbt $n &
        if ! ./tmp.light $i $out/$b.nbt $n; then
            echo "$b $n fail"
            #exit
        fi
    done
    wait
done
