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

BUILD_STATIC=1
BUILD_TYPE=Release

# determine if we are running from inside of
# the MetricBench repository or we are running
# as a free standing script.

SCRIPTPATH=$(cd $(dirname "$0"); pwd)
PARENTPATH=$(cd "${SCRIPTPATH}/.."; pwd)
GITCNF="${PARENTPATH}/.git/config"
MAKE_JOBS=$(grep -cw ^processor /proc/cpuinfo)

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
    add-apt-repository -y ppa:ubuntu-toolchain-r/test
    apt-get update
    apt-get install -y gcc g++
  else
    apt-get install -y gcc-4.8 g++-4.8
  fi

  apt-get install -y --force-yes \
    vim \
    bzr git wget \
    cmake make \
    libmysqlclient18 libmysqlclient-dev \
    libbz2-dev \
    scons

  mkdir ${BUILD_ROOT}

fi

# set compiler

if hash g++-4.8 2> /dev/null; then 
  export CC=/usr/bin/gcc-4.8
  export CXX=/usr/bin/g++-4.8
else
  export CC=/usr/bin/gcc
  export CXX=/usr/bin/g++
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
if [ ! -d "${BOOST_ROOT}" ] || \
   [ "$(find ${BOOST_ROOT} -type f \( -name '*.a' -or -name '*.so.*' \) | wc -l)" == "0" ]; then
  mkdir -p ${BOOST_ROOT}
  ./bootstrap.sh --prefix=${BOOST_ROOT} \
          --libdir=${BOOST_ROOT}/lib --includedir=${BOOST_ROOT}/include \
          --with-libraries=program_options,filesystem,system,test,thread,regex
  if [ "${BUILD_STATIC}" -gt 0 ]; then
    ./b2 -j${MAKE_JOBS} --build-type=complete --layout=tagged link=static install | tee b2.out
  else
    ./b2 -j${MAKE_JOBS} --build-type=complete --layout=tagged link=shared install | tee b2.out
  fi
fi
export BOOST_ROOT

# MySQL Connector C++ - 1.1.5

cd ${BUILD_ROOT}
if [ ! -d "${BUILD_ROOT}/mysql-connector-cpp" ]; then
  git clone https://github.com/dbpercona/mysql-connector-cpp.git
fi
cd mysql-connector-cpp
git checkout 1.1.7.db
MYSQLCONNECTORCPP_ROOT_DIR=${BUILD_ROOT}/MetricBench_mysqlcpp
if [ ! -d "${BUILD_ROOT}/MetricBench_mysqlcpp" ] || \
   [ "$(find ${BUILD_ROOT}/MetricBench_mysqlcpp/ \( -name '*.a' -or -name '*.so' \) | wc -l)" == "0" ]; then
  cmake -DMYSQLCLIENT_STATIC_BINDING:BOOL=1 -DCMAKE_INSTALL_PREFIX:PATH=${MYSQLCONNECTORCPP_ROOT_DIR} .
  make -j${MAKE_JOBS} install
fi
export MYSQLCONNECTORCPP_ROOT_DIR

# Mongo C++ driver

cd ${BUILD_ROOT}
if [ ! -d "${BUILD_ROOT}/mongo-cxx-driver" ]; then
  git clone https://github.com/mongodb/mongo-cxx-driver.git
fi
cd mongo-cxx-driver
git checkout legacy-1.1.0
MONGO_LIB_ROOT_DIR=${BUILD_ROOT}/MetricBench_mongo-cxx-driver
if [ ! -d "${MONGO_LIB_ROOT_DIR}" ] || \
   [ "$(find ${MONGO_LIB_ROOT_DIR} \( -name '*.a' -or -name '*.so' \) | wc -l)" == "0" ]; then
  if [ "${BUILD_STATIC}" -gt 0 ]; then
    scons -j${MAKE_JOBS} --cc=${CC} --cxx=${CXX} --prefix=${MONGO_LIB_ROOT_DIR} --extrapath=${BOOST_ROOT} --dynamic-boost=off --c++11 install
  else
    scons -j${MAKE_JOBS} --cc=${CC} --cxx=${CXX} --prefix=${MONGO_LIB_ROOT_DIR} --extrapath=${BOOST_ROOT} --dynamic-boost=on --c++11 install
  fi
fi
export MONGO_LIB_ROOT_DIR

# Cassandra (DataStax) C++ Driver

cd ${BUILD_ROOT}
if [ ! -d "${BUILD_ROOT}/cassandra-cxx-driver" ]; then
  git clone https://github.com/datastax/cpp-driver.git cassandra-cxx-driver
fi
cd cassandra-cxx-driver
git checkout 2.2.2
CASS_LIB_ROOT_DIR=${BUILD_ROOT}/MetricBench_cassandra-cxx-driver
if [ ! -d "${BUILD_ROOT}/MetricBench_cassandra-cxx-driver" ] || \
   [ "$(find ${BUILD_ROOT}/MetricBench_cassandra-cxx-driver/ \( -name '*.a' -or -name '*.so' \) | wc -l)" == "0" ]; then
  if [ "${BUILD_STATIC}" -gt 0 ]; then
    cmake -DCASS_BUILD_STATIC=ON -DCASS_USE_STATIC_LIBS=ON -DCMAKE_INSTALL_PREFIX:PATH=${CASS_LIB_ROOT_DIR} .
  else
    cmake -DCASS_BUILD_STATIC=OFF -DCASS_USE_STATIC_LIBS=OFF -DCMAKE_INSTALL_PREFIX:PATH=${CASS_LIB_ROOT_DIR} .
  fi
  make -j${MAKE_JOBS} install
fi
export CASS_LIB_ROOT_DIR

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
make -j${MAKE_JOBS}

# Epilogue 

echo "-----"
if [ -e $MBROOT/src/MetricBench ]; then
  echo "Build was successful: ${MBROOT}/src/MetricBench"
else
  echo "Build was unsuccessful"
fi
