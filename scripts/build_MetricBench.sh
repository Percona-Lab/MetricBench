#!/bin/bash

# build MetricBench
#
# Currently supports Ubuntu 12 / 14

# david.bennett at percona.com - 6/9/2015

# Note: you will need to run as root for the first section

BUILD_STATIC=0
BUILD_ROOT=${HOME}/build

# --- root section ---

# install package dependencies for building

apt-get update

HAS_GPP_48=$(apt-cache search 'g\+\+-4\.8' | head -n1 | awk '{print $1}')

if [ -z "${HAS_GPP_48}" ]; then
  apt-get install -y --force-yes python-software-properties
  add-apt-repository -y ppa:ubuntu-toolchain-r/test
  apt-get update
fi

apt-get install -y --force-yes \
  vim \
  bzr git wget \
  gcc-4.8 g++-4.8 \
  cmake make \
  libmysqlclient18 libmysqlclient-dev \
  libbz2-dev

# make gcc-4.8 the default

cd /usr/bin
for CMD in cpp gcc g++; do
  [ -L ${CMD} ] && rm ${CMD}
  ln -s ${CMD}-4.8 ${CMD}
done 

# --- user section (does not require root) ---

# download and install Boost 1.58

mkdir ${BUILD_ROOT}
cd ${BUILD_ROOT}
wget -q -O - \
        http://sf.net/projects/boost/files/boost/1.58.0/boost_1_58_0.tar.gz/download \
        | tar xzvf -
cd boost_1_58_0
BOOST_ROOT=${BUILD_ROOT}/MetricBench_boost
mkdir -p ${BOOST_ROOT}
./bootstrap.sh --prefix=${BOOST_ROOT} \
        --libdir=${BOOST_ROOT}/lib --includedir=${BOOST_ROOT}/include \
        --with-libraries=program_options,filesystem,system,test
if [ "${BUILD_STATIC}" -gt 0 ]; then
  ./b2 --build-type=complete --layout=tagged link=static install | tee b2.out
else
  ./b2 --build-type=complete --layout=tagged link=shared install | tee b2.out
fi
export BOOST_ROOT

# MySQL Connector C++ - 1.1.5

cd ${BUILD_ROOT}
bzr branch lp:~mysql/mysql-connector-cpp/trunk mysql-connector-cpp
cd mysql-connector-cpp
bzr revert -r1.1.5
MYSQLCONNECTORCPP_ROOT_DIR=${BUILD_ROOT}/MetricBench_mysqlcpp
cmake -DMYSQLCLIENT_STATIC_BINDING:BOOL=1 -DCMAKE_INSTALL_PREFIX:PATH=${MYSQLCONNECTORCPP_ROOT_DIR} .
make install
export MYSQLCONNECTORCPP_ROOT_DIR

# download and build Metric Bench using exported roots of boost, mysqlcppconn

cd ${BUILD_ROOT}
git clone https://github.com/Percona-Lab/MetricBench.git
cd MetricBench/src
./clean.sh all
if [ "${BUILD_STATIC}" -gt 0 ]; then
  cmake -DBUILD_STATIC=1 -DMYSQLCONNECTORCPP_ROOT_DIR:PATH=${MYSQLCONNECTORCPP_ROOT_DIR} .
else
  cmake -DMYSQLCONNECTORCPP_ROOT_DIR:PATH=${MYSQLCONNECTORCPP_ROOT_DIR} .
fi
make

# Epilogue 

echo "-----"
if [ -e $BUILD_ROOT/MetricBench/src/MetricBench ]; then
  echo "Build was successful: ${BUILD_ROOT}/MetricBench/src/MetricBench"
else
  echo "Build was unsuccessful"
fi
