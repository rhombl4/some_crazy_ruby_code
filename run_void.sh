#!/bin/sh

set -e

make void

bin=tmp.void
cp void ${bin}

#rm -fr out
mkdir -p out

for i in fmodels/FD*_src.mdl; do
    for n in `seq 1 40`; do
        out=out/$n
        mkdir -p $out
        b=$(basename $i | sed 's/_src\.mdl$//')
        echo $b

        if true; then
            ./${bin} $i $out/$b.nbt $n &
        else
            if ! ./${bin} $i $out/$b.nbt $n; then
                echo "$b $n fail"
                exit
            fi
        fi
    done
    wait
done
