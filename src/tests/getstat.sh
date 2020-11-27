#!/usr/bin/bash

make
./2tcp > 2out.txt
../parse_output tcp2.log -ascii > 2outlog.txt
cat 2outlog.txt | grep TCP_SINK > temp.txt