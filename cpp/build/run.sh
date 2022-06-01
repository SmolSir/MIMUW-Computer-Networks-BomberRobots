#!/bin/bash
cmake ..
make
./robots-client -d 127.0.0.1:10002 -n SmolSir -p 10022 -s students.mimuw.edu.pl:10112
#./robots-client -d localhost:10002 -n SmolSir -p 10022 -s students.mimuw.edu.pl:10200
#./robots-client -d 127.0.0.1:10002 -n SmolSir -p 10022 -s 2001:6a0:5001:1::3:10200