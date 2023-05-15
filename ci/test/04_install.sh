#!/usr/bin/env bash
#
# Copyright (c) 2018-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

if [[ $QEMU_USER_CMD == qemu-s390* ]]; then
  export LC_ALL=C
fi

if [ "$CI_OS_NAME" == "macos" ]; then
  sudo -H pip3 install --upgrade pip
  # shellcheck disable=SC2086
  IN_GETOPT_BIN="/usr/local/opt/gnu-getopt/bin/getopt" ${CI_RETRY_EXE} pip3 install --user $PIP_PACKAGES
fi

# Create folders that are mounted into the docker
mkdir -p "${CCACHE_DIR}"
mkdir -p "${PREVIOUS_RELEASES_DIR}"

export ASAN_OPTIONS="detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1"
export LSAN_OPTIONS="suppressions=${BASE_BUILD_DIR}/test/sanitizer_suppressions/lsan"
export TSAN_OPTIONS="suppressions=${BASE_BUILD_DIR}/test/sanitizer_suppressions/tsan:halt_on_error=1"
export UBSAN_OPTIONS="suppressions=${BASE_BUILD_DIR}/test/sanitizer_suppressions/ubsan:print_stacktrace=1:halt_on_error=1:report_error_type=1"
env | grep -E '^(BASE_|QEMU_|CCACHE_|LC_ALL|BOOST_TEST_RANDOM|DEBIAN_FRONTEND|CONFIG_SHELL|(ASAN|LSAN|TSAN|UBSAN)_OPTIONS|PREVIOUS_RELEASES_DIR))' | tee /tmp/env
if [[ $BITCOIN_CONFIG = *--with-sanitizers=*address* ]]; then # If ran with (ASan + LSan), Docker needs access to ptrace (https://github.com/google/sanitizers/issues/764)
  DOCKER_ADMIN="--cap-add SYS_PTRACE"
fi

export P_CI_DIR="$PWD"
export BINS_SCRATCH_DIR="${BASE_SCRATCH_DIR}/bins/"

if [ -z "$DANGER_RUN_CI_ON_HOST" ]; then
  echo "Creating $DOCKER_NAME_TAG container to run in"
  LOCAL_UID=$(id -u)
  LOCAL_GID=$(id -g)

  # the name isn't important, so long as we use the same UID
  LOCAL_USER=nonroot
  ${CI_RETRY_EXE} docker pull "$DOCKER_NAME_TAG"

  # shellcheck disable=SC2086
  DOCKER_ID=$(docker run $DOCKER_ADMIN -idt \
                  --mount type=bind,src=$BASE_ROOT_DIR,dst=/ro_base,readonly \
                  --mount type=bind,src=$CCACHE_DIR,dst=$CCACHE_DIR \
                  --mount type=bind,src=$DEPENDS_DIR,dst=$DEPENDS_DIR \
                  --mount type=bind,src=$PREVIOUS_RELEASES_DIR,dst=$PREVIOUS_RELEASES_DIR \
                  -w $BASE_ROOT_DIR \
                  --env-file /tmp/env \
                  --name $CONTAINER_NAME \
                  $DOCKER_NAME_TAG)

  # Create a non-root user inside the container which matches the local user.
  #
  # This prevents the root user in the container modifying the local file system permissions
  # on the mounted directories
  docker exec "$DOCKER_ID" useradd -u "$LOCAL_UID" -o -m "$LOCAL_USER"
  docker exec "$DOCKER_ID" groupmod -o -g "$LOCAL_GID" "$LOCAL_USER"
  docker exec "$DOCKER_ID" chown -R "$LOCAL_USER":"$LOCAL_USER" "${BASE_ROOT_DIR}"
  export DOCKER_CI_CMD_PREFIX_ROOT="docker exec -u 0 $DOCKER_ID"
  export DOCKER_CI_CMD_PREFIX="docker exec -u $LOCAL_UID $DOCKER_ID"
else
  echo "Running on host system without docker wrapper"
fi

CI_EXEC () {
  $DOCKER_CI_CMD_PREFIX bash -c "export PATH=${BINS_SCRATCH_DIR}:\$PATH && cd \"$P_CI_DIR\" && $*"
}
CI_EXEC_ROOT () {
  $DOCKER_CI_CMD_PREFIX_ROOT bash -c "export PATH=${BINS_SCRATCH_DIR}:\$PATH && cd \"$P_CI_DIR\" && $*"
}
export -f CI_EXEC
export -f CI_EXEC_ROOT

CI_EXEC mkdir -p "${BINS_SCRATCH_DIR}"