FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

# Create a user with sudo rights
RUN apt-get update && apt-get -y install sudo
RUN useradd -m sdr && echo "sdr:sdr" | chpasswd && adduser sdr sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
USER sdr

# Prepare buiid and install environment
RUN sudo mkdir /opt/build /opt/install \
    && sudo chown sdr:sdr /opt/build /opt/install

# Configure tzdata manually
ENV TZONE=Europe/Paris
RUN sudo ln -fs /usr/share/zoneinfo/$TZONE /etc/localtime \
    && sudo apt-get update && sudo apt-get -y install tzdata

# Install base build packages dependencies - step 1
RUN sudo apt-get update && sudo apt-get -y install \
    git \
    cmake \
    g++ \
    pkg-config \
    autoconf \
    automake \
    libtool \
    libfftw3-dev \
    libusb-1.0-0-dev \
    libusb-dev

# Install base build packages dependencies - Qt5
RUN sudo apt-get update && sudo apt-get -y install \
    qt5-default \
    qtbase5-dev \
    qtchooser \
    libqt5multimedia5-plugins \
    qtmultimedia5-dev \
    qttools5-dev \
    qttools5-dev-tools \
    libqt5opengl5-dev \
    qtbase5-dev

# Install base build packages dependencies - Boost
RUN sudo apt-get update && sudo apt-get -y install \
    libboost-all-dev

# Install base build packages dependencies - the rest
RUN sudo apt-get update && sudo apt-get -y install \
    libasound2-dev \
    pulseaudio \
    libopencv-dev \
    libxml2-dev \
    bison \
    flex \
    ffmpeg \
    libavcodec-dev \
    libavformat-dev \
    libopus-dev

# CM256cc
RUN cd /opt/build \
    && git clone https://github.com/f4exb/cm256cc.git \
    && cd cm256cc \
    && git reset --hard 64beaaa \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/cm256cc .. \
    && make -j4 install

# MBElib
RUN cd /opt/build \
    && git clone https://github.com/szechyjs/mbelib.git \
    && cd mbelib \
    && git reset --hard e2d84c1 \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/mbelib .. \
    && make -j4 install

# SerialDV
RUN cd /opt/build \ 
    && git clone https://github.com/f4exb/serialDV.git \
    && cd serialDV \
    && git reset --hard 06caac6 \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/serialdv .. \
    && make -j4 install
    
# DSDcc    
RUN cd /opt/build \
    && git clone https://github.com/f4exb/dsdcc.git \
    && cd dsdcc \
    && git reset --hard 2a89df4 \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/dsdcc -DUSE_MBELIB=ON -DLIBMBE_INCLUDE_DIR=/opt/install/mbelib/include -DLIBMBE_LIBRARY=/opt/install/mbelib/lib/libmbe.so -DLIBSERIALDV_INCLUDE_DIR=/opt/install/serialdv/include/serialdv -DLIBSERIALDV_LIBRARY=/opt/install/serialdv/lib/libserialdv.so .. \
    && make -j4 install

# Codec2
RUN sudo apt-get update && sudo apt-get -y install subversion \
    libspeexdsp-dev \
    libsamplerate0-dev
RUN cd /opt/build \
    && svn co https://svn.code.sf.net/p/freetel/code/codec2-dev@4067 codec2-dev \
    && cd codec2-dev \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/codec2 .. \
    && make -j4 install
    
# Airspy
RUN cd /opt/build \
    && git clone https://github.com/airspy/host.git libairspy \
    && cd libairspy \
    && git reset --hard 5c86e53 \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/libairspy .. \
    && make -j4 install

# RTL-SDR
RUN cd /opt/build \
    && git clone https://github.com/librtlsdr/librtlsdr.git \
    && cd librtlsdr \
    && git reset --hard c7d970a \
    && mkdir build; cd build \
    && cmake -Wno-dev -DDETACH_KERNEL_DRIVER=ON -DCMAKE_INSTALL_PREFIX=/opt/install/librtlsdr .. \
    && make -j4 install

# PlutoSDR
RUN cd /opt/build \
    && git clone https://github.com/analogdevicesinc/libiio.git \
    && cd libiio \
    && git reset --hard 5bdc242 \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/libiio -DINSTALL_UDEV_RULE=OFF .. \
    && make -j4 install

# BladeRF
RUN cd /opt/build \
    && git clone https://github.com/Nuand/bladeRF.git \
    && cd bladeRF/host \
    && git reset --hard 32058c4 \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/libbladeRF -DINSTALL_UDEV_RULES=OFF .. \
    && make -j4 install
    
# HackRF
RUN cd /opt/build \
    && git clone https://github.com/mossmann/hackrf.git \
    && cd hackrf/host \
    && git reset --hard 9bbbbbf \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/libhackrf -DINSTALL_UDEV_RULES=OFF .. \
    && make -j4 install

# LimeSDR
RUN cd /opt/build \
    && git clone https://github.com/myriadrf/LimeSuite.git \
    && cd LimeSuite \
    && git reset --hard 59d51d5 \
    && mkdir builddir; cd builddir \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/LimeSuite .. \
    && make -j4 install

# Airspy HF
RUN cd /opt/build \
    && git clone https://github.com/airspy/airspyhf \
    && cd airspyhf \
    && git reset --hard 075b8f9 \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/libairspyhf .. \
    && make -j4 install
    
# Perseus
RUN sudo apt-get update && sudo apt-get -y install xxd
RUN cd /opt/build \
    && git clone https://github.com/f4exb/libperseus-sdr.git \
    && cd libperseus-sdr \
    && git checkout fixes \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/libperseus .. \
    && make \
    && make install    
    
# XTRX
RUN sudo apt-get update && sudo apt-get -y install python-cheetah
RUN cd /opt/build \
    && git clone https://github.com/xtrx-sdr/images.git xtrx-images \
    && cd xtrx-images \
    && git reset --hard 053ec82 \
    && git submodule init \
    && git submodule update \
    && cd sources \
    && mkdir build; cd build \
    && cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=/opt/install/xtrx-images -DENABLE_SOAPY=NO .. \
    && make -j4 install    

# Soapy main
RUN cd /opt/build \
    && git clone https://github.com/pothosware/SoapySDR.git \
    && cd SoapySDR \
    && git reset --hard 6d21756 \
    && mkdir build; cd build \
    && cmake -DCMAKE_INSTALL_PREFIX=/opt/install/SoapySDR .. \
    && make -j4 install

# Soapy remote
RUN sudo apt-get update && sudo apt-get -y install libavahi-client-dev
RUN cd /opt/build \
    && git clone https://github.com/pothosware/SoapyRemote.git \
    && cd SoapyRemote \
    && git reset --hard 4f5d717 \
    && mkdir build; cd build \
    && cmake -DCMAKE_INSTALL_PREFIX=/opt/install/SoapySDR -DSOAPY_SDR_INCLUDE_DIR=/opt/install/SoapySDR/include -DSOAPY_SDR_LIBRARY=/opt/install/SoapySDR/lib/libSoapySDR.so .. \
    && make -j4 install
    
# Finally SDRangel
RUN cd /opt/build \
    && git clone https://github.com/f4exb/sdrangel.git \
    && cd sdrangel \
    && mkdir build; cd build \
    && cmake -Wno-dev -DDEBUG_OUTPUT=ON -DBUILD_TYPE=RELEASE -DRX_SAMPLE_24BIT=ON -DMIRISDR_DIR=/opt/install/libmirisdr -DAIRSPY_DIR=/opt/install/libairspy -DAIRSPYHF_DIR=/opt/install/libairspyhf -DBLADERF_DIR=/opt/install/libbladeRF -DHACKRF_DIR=/opt/install/libhackrf -DRTLSDR_DIR=/opt/install/librtlsdr -DLIMESUITE_DIR=/opt/install/LimeSuite -DIIO_DIR=/opt/install/libiio -DCM256CC_DIR=/opt/install/cm256cc -DDSDCC_DIR=/opt/install/dsdcc -DSERIALDV_DIR=/opt/install/serialdv -DMBE_DIR=/opt/install/mbelib -DPERSEUS_DIR=/opt/install/libperseus -DXTRX_DIR=/opt/install/xtrx-images -DSOAPYSDR_DIR=/opt/install/SoapySDR -DCMAKE_INSTALL_PREFIX=/opt/install/sdrangel .. \
    && make -j8 install
