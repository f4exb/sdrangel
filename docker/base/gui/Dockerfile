FROM sdrangel/bionic:base

# Finally SDRangel
RUN cd /opt/build \
    && git clone https://github.com/f4exb/sdrangel.git \
    && cd sdrangel \
    && git reset --hard v4.5.0 \
    && mkdir build; cd build \
    && cmake -Wno-dev -DDEBUG_OUTPUT=ON -DBUILD_TYPE=RELEASE -DRX_SAMPLE_24BIT=ON -DBUILD_SERVER=OFF -DMIRISDR_DIR=/opt/install/libmirisdr -DAIRSPY_DIR=/opt/install/libairspy -DAIRSPYHF_DIR=/opt/install/libairspyhf -DBLADERF_DIR=/opt/install/libbladeRF -DHACKRF_DIR=/opt/install/libhackrf -DRTLSDR_DIR=/opt/install/librtlsdr -DLIMESUITE_DIR=/opt/install/LimeSuite -DIIO_DIR=/opt/install/libiio -DCM256CC_DIR=/opt/install/cm256cc -DDSDCC_DIR=/opt/install/dsdcc -DSERIALDV_DIR=/opt/install/serialdv -DMBE_DIR=/opt/install/mbelib -DCODEC2_DIR=/opt/install/codec2 -DPERSEUS_DIR=/opt/install/libperseus -DXTRX_DIR=/opt/install/xtrx-images -DSOAPYSDR_DIR=/opt/install/SoapySDR -DCMAKE_INSTALL_PREFIX=/opt/install/sdrangel .. \
    && make -j8 install

# Configure SSH for X-forwarding to be able to start the UI from ssh connection
RUN sudo sed -i '/X11Forwarding/c\X11Forwarding yes' /etc/ssh/sshd_config \
	&& sudo sed -i '/X11UseLocalhost/c\X11UseLocalhost no' /etc/ssh/sshd_config
