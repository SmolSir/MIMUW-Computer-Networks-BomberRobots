#!/bin/bash
cd build
cmake ..
make
./robots-server         \
    -b 5                \
    -c 2                \
    -d 1000             \
    -e 2                \
    -k 10               \
    -l 30               \
    -n "test-server"    \
    -p 10222            \
    -s 1                \
    -x 5                \
    -y 5