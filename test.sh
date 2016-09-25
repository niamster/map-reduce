#!/bin/bash

# run static analyzer
./waf clean
scan-build ./waf configure || exit $?
./waf || exit $?

san=(address,undefined thread)

# can't use clang with recent glibc (https://bugs.archlinux.org/task/50366)
./waf clean
./waf configure
./waf
while true; do
    for s in ${san[@]}; do
        ./waf --san=$s --mode=debug || exit $?
        for t in ./build/test*; do
            [ -x $t -a -f $t ] || continue
            echo "=============================="
            echo "Running $(basename $t) with $s"
            $t || exit $?
            echo "=============================="
            sleep 0.5
        done
    done
done
