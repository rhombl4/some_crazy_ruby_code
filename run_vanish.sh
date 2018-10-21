#!/bin/sh

set -e

make vanish

bin=tmp.vanish
cp vanish ${bin}

for i in fmodels/F*_src.mdl; do
    n=8
    out=out/vanish
    mkdir -p $out
    b=$(basename $i | sed 's/_src\.mdl$//')
    echo $b

    if false; then
        ./${bin} $i $out/$b.nbt $n &
    else
        if ! ./${bin} $i $out/$b.nbt $n 2> logs/$b; then
            echo "$b $n fail"
            #exit
        fi
    fi
done
