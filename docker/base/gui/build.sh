#!/bin/sh

IMAGE_NAME=sdrangel/bionic:gui
docker build -t ${IMAGE_NAME} .
