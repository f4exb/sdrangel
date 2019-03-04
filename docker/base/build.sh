#!/bin/sh

IMAGE_NAME=sdrangel/bionic:base
docker build -t ${IMAGE_NAME} .
