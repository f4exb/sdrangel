FROM sdrangel/bionic:gui

RUN sudo apt-get update && sudo apt-get install -y mesa-utils binutils kmod

# install nvidia driver
ADD NVIDIA-DRIVER.run /tmp/NVIDIA-DRIVER.run
RUN sudo sh /tmp/NVIDIA-DRIVER.run -s --ui=none --no-kernel-module --install-libglvnd --no-questions
RUN sudo rm /tmp/NVIDIA-DRIVER.run

# debug
RUN sudo apt-get update && sudo apt-get install -y gdb alsa-utils
RUN sudo usermod -a -G adm sdr

# Start SDRangel and some more services on which SDRangel depends
COPY start.sh /start.sh
ENTRYPOINT ["/start.sh"]
