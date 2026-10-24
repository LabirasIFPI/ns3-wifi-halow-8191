#!/bin/sh
set -eu

IMAGE_NAME=${HALOW_DOCKER_IMAGE:-ns3-halow-env}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

docker build -t "$IMAGE_NAME" -f "$ROOT/docker/Dockerfile" "$ROOT"
