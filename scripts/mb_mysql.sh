#!/bin/bash

basedir=$(cd $(dirname "$0"); pwd)

mode=$1;

if [ "${mode}" = "" ]; then
  mode="prepare";
else
  shift;
fi

${basedir}/../src/MetricBench --driver mysql \
  --mode "${mode}" \
  --csvstats latencystats.csv \
  --csvstream sampledstats.csv \
  "$@"

