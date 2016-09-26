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

rtests() {
    for t in ./build/test*; do
        [ -x $t -a -f $t ] || continue
        echo "=============================="
        echo "Running $(basename $t) with $*"
        $t || exit $?
        echo "=============================="
        sleep 0.5
    done
}

while true; do
    for s in ${san[@]}; do
        ./waf --san=$s --mode=debug || exit $?
        rtests $s
    done
    ./waf --mode=debug || exit $?
    rtests $s
    ./waf || exit $?
    rtests $s
    [ "x$1" = "xonce" ] && break
done
