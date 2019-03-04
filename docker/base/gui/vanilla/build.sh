#!/bin/sh

IMAGE_NAME=sdrangel/bionic:vanilla
docker build -t ${IMAGE_NAME} .
