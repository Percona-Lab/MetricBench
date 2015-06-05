#!/bin/bash

cd `dirname $0`
find . -type f \( -name '*.cpp' -or -name '*.hpp' \) -exec cppcheck --force {} \;
./clean.sh bin
scan-build -o . make -j4

