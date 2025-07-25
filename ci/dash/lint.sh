#!/usr/bin/env bash
# Wrapper script to run lint - created to fix CI path issue
cd "$(dirname "$0")/../.."
exec ./ci/lint_run_all.sh "$@"