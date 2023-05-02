#!/usr/bin/env bash
#
# Copyright (c) 2019-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export PACKAGES="systemtap-sdt-dev clang-16 llvm-16 libclang-rt-16-dev python3-zmq qtbase5-dev qttools5-dev qttools5-dev-tools libevent-dev bsdmainutils libboost-dev libdb5.3++-dev libminiupnpc-dev libnatpmp-dev libzmq3-dev libqrencode-dev libsqlite3-dev"
export NO_DEPENDS=1
export TEST_RUNNER_EXTRA="--timeout-factor=4"  # Increase timeout because sanitizers slow down
export FUNCTIONAL_TESTS_CONFIG="--exclude wallet_multiwallet.py" # Temporarily suppress ASan heap-use-after-free (see issue #14163)
export RUN_BENCH=true
export GOAL="install"
export BITCOIN_CONFIG="--enable-c++20 --enable-usdt --enable-zmq --with-incompatible-bdb --with-gui=qt5 CPPFLAGS='-DARENA_DEBUG -DDEBUG_LOCKORDER' --with-sanitizers=address,integer,undefined CC=clang-16 CXX=clang++-16 --with-boost-process"