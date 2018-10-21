#!/bin/sh

set -e

make full

bin=tmp.full_fr
cp full ${bin}

#rm -fr out
#mkdir -p out

for i in fmodels/FR*_tgt.mdl; do
    for n in `seq 1 40`; do
        for ox in -1 0 1; do
            for oz in -1 0 1; do
                out=fr_out/tgt_${n}_${ox}_${oz}
                mkdir -p $out
                b=$(basename $i | sed 's/_tgt\.mdl$//')
                echo $b
                G_T_OFF_X=${ox} G_T_OFF_Z=${oz} ./${bin} $i $out/$b.nbt $n &
                #if ! ./tmp.full $i $out/$b.nbt $n; then
                #    echo "$b $n fail"
                #fi
            done
            wait
        done
    done
done
