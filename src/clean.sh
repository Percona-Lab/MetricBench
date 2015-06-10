#!/bin/bash

cd `dirname $0`

case $1 in

  all)
    rm -rf CMakeCache.txt CMakeFiles/ Makefile MetricBench cmake_install.cmake .Makefile.swp testBin
    ;;
  bin*)
    find . -type f -exec file {} \; | grep ELF | cut -d':' -f1 | xargs rm
    ;;
  *)
    echo $"Usage: $0 {all|binary}"
    exit 1

esac
