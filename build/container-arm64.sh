#!/bin/bash
set -euxo pipefail

export IMAGE_NAME="tmbasic-build-arm64"
export HOST_UID=$(id -u "$USER")
export HOST_GID=$(id -g "$USER")
export BASE_IMAGE_NAME="arm64v8/ubuntu:20.04"

[ $(uname -m) == "aarch64" ] || docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

cat docker/Dockerfile.build | envsubst | docker build -t $IMAGE_NAME docker -f-

pushd ..
docker run --rm --tty --interactive --volume "$PWD:/code" --workdir /code $IMAGE_NAME
popd