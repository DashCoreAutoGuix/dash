#!/usr/bin/env bash
#
# Copyright (c) 2019-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export HOST=s390x-linux-gnu
export PACKAGES="python3-zmq"
export CONTAINER_NAME=ci_s390x
export CI_IMAGE_NAME_TAG="docker.io/s390x/debian:bookworm"
export RUN_UNIT_TESTS=true
export TEST_RUNNER_ENV="LC_ALL=C"
export TEST_RUNNER_EXTRA="--exclude feature_init,rpc_bind,feature_bind_extra"  # Excluded for now, see https://github.com/bitcoin/bitcoin/issues/17765#issuecomment-602068547export RUN_FUNCTIONAL_TESTS=true
export GOAL="install"
export BITCOIN_CONFIG="--enable-reduce-exports --disable-gui-tests --with-boost-process"  # GUI tests disabled for now, see https://github.com/bitcoin/bitcoin/issues/23730
