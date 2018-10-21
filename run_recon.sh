#!/bin/bash

set -e

make recon

bin=tmp.recon
cp recon ${bin}

#rm -fr out
mkdir -p out

run() {
    f=$1
    t=$2
    for n in `seq $f $t`; do
        out=out/$n
        mkdir -p $out
        b=$(basename $i | sed 's/_src\.mdl$//')
        t=fmodels/${b}_tgt.mdl
        echo $b

        if true; then
            ./${bin} $i $t $out/$b.nbt $n &
        else
            if ! ./${bin} $i $t $out/$b.nbt $n; then
                echo "$b $n fail"
                #exit
            fi
        fi
    done
    wait
}

for i in fmodels/FR*_src.mdl; do
    n=$(echo $i | sed 's/.*FR0*// ; s/_.*//')
    if (( $n > 115 )); then
        run 1 8
        run 9 16
        run 17 24
        run 25 32
        run 33 40
    fi

    #run 1 40
    # run 1 8
    # run 9 16
    # run 17 24
    # run 25 32
    # run 33 40
done
