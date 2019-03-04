#!/bin/sh

IMAGE_NAME=sdrangel/bionic:server
docker build -t ${IMAGE_NAME} .
