#!/bin/bash

# this will cleanup the MetricBench tables

# Usage: ./clean_database.sh {-h host} {-P port} {-u user} {-p password} database

HOST=localhost
PORT=3306
USER=root
PASSWORD=
DATABASE=test
PREFIX=metrics

while getopts :h:P:u:p: opt; do
  case $opt in
    h)
      HOST=$OPTARG
     ;;
    P)
      PORT=$OPTARG
      ;; 
    u)
      USER=$OPTARG
      ;;
    p)
      PASSWORD=$OPTARG
      ;;
  esac
done

# remainder is database name
shift "$((OPTIND - 1))"

if [ ! "$1" = "" ]; then
  DATABASE=$1
fi

if [ ! "${PASSWORD}" = "" ]; then
  pwopt="-p${PASSWORD}"
else
  pwopt=""
fi

mysql_cmd="mysql -h${HOST} -P${PORT} -u${USER} ${pwopt} ${DATABASE} $@"

# echo "cmd: ${mysql_cmd}"

# validate command

if ${mysql_cmd} -e 'show tables' > /dev/null 2>&1 ; then

  ${mysql_cmd} -e 'show tables' | \
      grep "^${PREFIX}[0-9]" | \
      xargs -n1 -i^ ${mysql_cmd} -e "drop table ^"

else
  echo "Error exec: ${mysql_cmd}"
fi

