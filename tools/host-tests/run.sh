#!/bin/sh
# Host-side logic tests (Linux, gcc). G = glide3x folder of the patched tree.
G=${1:-../../sources/3dfx_glide_sourceforge-3.10.0.40409/glide3x}
gcc -O1 -Wall -DH3 -DFX_GLIDE_NAPALM=1 -Istub -I$G/h5/incsrc -I$G/h5/minihwc \
    -I$G/swlibs/fxmisc -I$G/swlibs/newpci/pcilib -o harness harness.c && ./harness
gcc -O1 -o harness2 harness2.c && ./harness2
