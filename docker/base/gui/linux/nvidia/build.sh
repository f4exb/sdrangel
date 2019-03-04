#!/bin/sh

IMAGE_NAME=sdrangel/bionic:linux_nvidia
NVIDIA_VER=$(nvidia-smi --query-gpu=driver_version --format=csv,noheader) #410.78
NVIDIA_DRIVER=NVIDIA-Linux-x86_64-${NVIDIA_VER}.run  # path to nvidia driver

if [ ! -f ${NVIDIA_DRIVER} ]; then
    wget http://us.download.nvidia.com/XFree86/Linux-x86_64/${NVIDIA_VER}/NVIDIA-Linux-x86_64-${NVIDIA_VER}.run
    cp ${NVIDIA_DRIVER} NVIDIA-DRIVER.run
fi

docker build -t ${IMAGE_NAME} .
