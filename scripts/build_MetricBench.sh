#!/bin/bash

# build MetricBench on a barebones Ubuntu 12/14 
# this script does everything, installs toolchain
# and dependencies, downloads and builds
# MySQL Connector C++ and Boost
# and downloads and builds MetricBench.
# You can run this on a clean docker or vagrant image
# of Ubuntu 12/14 x86_64
#
# If you are building somewhere else or just need part
# of this script it should be easy as pie to copy & paste
#
# Currently supports Ubuntu 12 / 14

# NOTE:  This script is designed to run as root
# in a docker container or bare bones VM

# david.bennett at percona.com - 6/9/2015

BUILD_STATIC=0
BUILD_TYPE=Debug

# determine if we are running from inside of
# the MetricBench repository or we are running
# as a free standing script.

SCRIPTPATH=$(cd $(dirname "$0"); pwd)
PARENTPATH=$(cd "${SCRIPTPATH}/.."; pwd)
GITCNF="${PARENTPATH}/.git/config"

if [ -f "${GITCNF}" ] && grep -Fq MetricBench "${GITCNF}"; then
  BUILD_ROOT="${PARENTPATH}/build"
  INREPO=1
else
  BUILD_ROOT="${SCRIPTPATH}/build"
  INREPO=0
fi

# install package dependencies for building

if [ ! -d "${BUILD_ROOT}" ]; then

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

  mkdir ${BUILD_ROOT}

fi

# download and install Boost 1.58

cd ${BUILD_ROOT}

if [ ! -d "boost_1_58_0" ]; then
  wget -q -O - \
        http://sf.net/projects/boost/files/boost/1.58.0/boost_1_58_0.tar.gz/download \
        | tar xzvf -
fi
cd boost_1_58_0
BOOST_ROOT=${BUILD_ROOT}/MetricBench_boost
if [ ! -e "${BOOST_ROOT}/lib/libboost_exception.a" ]; then
  mkdir -p ${BOOST_ROOT}
  ./bootstrap.sh --prefix=${BOOST_ROOT} \
          --libdir=${BOOST_ROOT}/lib --includedir=${BOOST_ROOT}/include \
          --with-libraries=program_options,filesystem,system,test,thread,regex
  if [ "${BUILD_STATIC}" -gt 0 ]; then
    ./b2 --build-type=complete --layout=tagged link=static install | tee b2.out
  else
    ./b2 --build-type=complete --layout=tagged link=shared install | tee b2.out
  fi
fi
export BOOST_ROOT

# MySQL Connector C++ - 1.1.5

cd ${BUILD_ROOT}
if [ ! -d "${BUILD_ROOT}/mysql-connector-cpp" ]; then
  bzr branch lp:~mysql/mysql-connector-cpp/trunk mysql-connector-cpp
fi
cd mysql-connector-cpp
bzr revert -r1.1.5
MYSQLCONNECTORCPP_ROOT_DIR=${BUILD_ROOT}/MetricBench_mysqlcpp
if [ "$(find ${BUILD_ROOT}/MetricBench_mysqlcpp/ \( -name '*.a' -or -name '*.so' \) | wc -l)" == "0" ]; then
  cmake -DMYSQLCLIENT_STATIC_BINDING:BOOL=1 -DCMAKE_INSTALL_PREFIX:PATH=${MYSQLCONNECTORCPP_ROOT_DIR} .
  make install
fi
export MYSQLCONNECTORCPP_ROOT_DIR

# Mongo C++ driver

cd ${BUILD_ROOT}
if [ ! -d "${BUILD_ROOT}/mongo-cxx-driver" ]; then
  git clone https://github.com/mongodb/mongo-cxx-driver.git
fi
cd mongo-cxx-driver
git checkout legacy
MONGO_LIB_ROOT_DIR=${BUILD_ROOT}/MetricBench_mongo-cxx-driver
if [ "$(find ${MONGO_LIB_ROOT_DIR} \( -name '*.a' -or -name '*.so' \) | wc -l)" ]; then 
  if [ "${BUILD_STATIC}" -gt 0 ]; then
    scons --prefix=${MONGO_LIB_ROOT_DIR} --extrapath=${BOOST_ROOT} --dynamic-boost=off  --c++11 install
  else
    scons --prefix=${MONGO_LIB_ROOT_DIR} --extrapath=${BOOST_ROOT} --dynamic-boost=on  --c++11 install
  fi
fi
export MONGO_LIB_ROOT_DIR

# download and build Metric Bench using exported roots of boost, mysqlcppconn

if [ "${INREPO}" -eq "1" ]; then
  MBROOT="${PARENTPATH}"
else
  cd ${BUILD_ROOT}
  if [ ! -d "${BUILD_ROOT}/MetricBench" ]; then
    git clone https://github.com/Percona-Lab/MetricBench.git
  fi
  MBROOT="${BUILD_ROOT}/MetricBench"
fi
cd ${MBROOT}/src
./clean.sh all
if [ "${BUILD_STATIC}" -gt 0 ]; then
  cmake -DBUILD_STATIC=1 -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DMYSQLCONNECTORCPP_ROOT_DIR:PATH=${MYSQLCONNECTORCPP_ROOT_DIR} .
else
  cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DMYSQLCONNECTORCPP_ROOT_DIR:PATH=${MYSQLCONNECTORCPP_ROOT_DIR} \
        -DMONGO_LIB_ROOT_DIR:PATH=${MONGO_LIB_ROOT_DIR}  .
fi
make -j4

# Epilogue 

echo "-----"
if [ -e $MBROOT/src/MetricBench ]; then
  echo "Build was successful: ${MBROOT}/src/MetricBench"
else
  echo "Build was unsuccessful"
fi
