#!/bin/bash

set -e

make void

bin=tmp.void
cp void ${bin}

#rm -fr out
#mkdir -p out

for i in fmodels/FR*_src.mdl; do
    n=$(echo $i | sed 's/.*FR0*// ; s/_.*//')
    if (( $n < 48 )); then
       continue
    fi

    for n in `seq 1 40`; do
        out=fr_out/src_${n}
        mkdir -p $out
        b=$(basename $i | sed 's/_src\.mdl$//')
        echo "$b $n"
        if [ ! -e $out/$b.nbt ]; then
            ./${bin} $i $out/$b.nbt $n
        fi
    done
    wait
done
