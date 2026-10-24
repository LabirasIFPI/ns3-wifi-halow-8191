#!/bin/sh
set -eu

IMAGE_NAME=${HALOW_DOCKER_IMAGE:-ns3-halow-env}
BUILD_DIR=${HALOW_BUILD_DIR:-build-o2}
JOBS=${HALOW_BUILD_JOBS:-2}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

docker run --rm \
  -e CXXFLAGS='-O2 -std=c++11' \
  -v "$ROOT:/workspace/IEEE-802.11ah-ns-3" \
  -w /workspace/IEEE-802.11ah-ns-3 \
  "$IMAGE_NAME" \
  ./waf configure --out="$BUILD_DIR" --build-profile=debug \
    --disable-examples --disable-tests

docker run --rm \
  -v "$ROOT:/workspace/IEEE-802.11ah-ns-3" \
  -w /workspace/IEEE-802.11ah-ns-3 \
  "$IMAGE_NAME" \
  ./waf build -j"$JOBS"
