![SDR Angel banner](doc/img/sdrangel_banner.png)

**SDRangel** is an Open Source Qt5 / OpenGL 3.0+ (Linux) 4.3+ (Windows 64) SDR and signal analyzer frontend to various hardware.

**Check the discussion group** [here](https://groups.io/g/sdrangel)

<h1>Source code</h1>

<h2>Repository branches</h2>

- master: the production branch
- dev: the development branch
- legacy: the modified code from the parent application [hexameron rtl-sdrangelove](https://github.com/hexameron/rtl-sdrangelove) before a major redeisign of the code was carried out and sync was lost.

<h2>Untested plugins</h2>

These plugins come from the parent code base and have been maintained so that they compile but they are not being actively tested:

channelrx:

  - demodlora
  - tcpsrc (although it has evolved please use the udpsrc plugin instead)

<h2>Deprecated plugins</h2>

These plugins are still present at least in the code but have been superceded:

  - chanalyzer: the Channel Analyzer channel plugin is superceded by the "new generation" Channel Analyzer NG (chanalyzerng)

<h1>Specific features</h1>

<h2>Multiple device support</h2>

From version 2 SDRangel can integrate more than one hardware device running concurrently.

<h2>Transmission support</h2>

From version 3 transmission or signal generation is supported for BladeRF, HackRF (since version 3.1), LimeSDR (since version 3.4) and PlutoSDR (since version 3.7.8) using a sample sink plugin. These plugins are:

  - [BladeRF output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/bladerfoutput) limited support in Windows
  - [HackRF output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/hackrfoutput)
  - [LimeSDR output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/limesdroutput)
  - [PlutoSDR output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/plutosdroutput)
  - [File output or file sink plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/filesink)
  - [Remote device via Network with SDRdaemon](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/sdrdaemonsink) Linux only

<h1>Notes on pulseaudio setup</h1>

The audio devices with Qt are supported through pulseaudio and unless you are using a single sound chip (or card) with a single output port or you are an expert with pulseaudio config files you may get into trouble when trying to route the audio to a different output port. These notes are a follow-up of issue #31 with my own experiments with HDMI audio output on the Udoo x86 board. So using this example of HDMI output you can do the following:

  - Install pavucontrol. It is included in most distributions for example:
    - Ubuntu/Debian: `sudo apt-get install pavucontrol`
    - openSUSE: `sudo zypper in pavucontrol`
  - Check the audio config with alsamixer. On the Udoo x86 the HDMI output depends on the S/PDIF control and it occasionally gets muted when the HDMI monitor is turned off or goes to sleep. So in any case make sure nothing is muted there.
  - Open pavucontrol and open the last tab (rightmost) called 'Configuration'
  - Select HDMI from the profiles list in the 'Configuration' tab
  - Then in the 'Output devices' tab the HDMI / display port is selected as it is normally the only one. Just double check this
  - In SDRangel's Preferences/Audio menu make sure something like `alsa_output.pci-0000_00_1b.0.hdmi-stereo` is selected. The default might also work after the pulseaudio configuration you just made.
  
In case you cannot see anything related to HDMI or your desired audio device in pavucontrol just restart pulseaudio with `pulseaudio -k` (`-k` kills the previous instance before restarting) and do the above steps again.

<h1>Supported hardware</h1>

<h2>Airspy</h2>

[Airspy R2](https://airspy.com/airspy-r2/) and [Airspy Mini](https://airspy.com/airspy-mini/) are supported through the libairspy library that should be installed in your system for proper build of the software and operation support. Add `libairspy-dev` to the list of dependencies to install.

If you use your own location for libairspy install directory you need to specify library and include locations. Example with `/opt/install/libairspy` with the following defines on `cmake` command line:

`-DLIBAIRSPY_LIBRARIES=/opt/install/libairspy/lib/libairspy.so -DLIBAIRSPY_INCLUDE_DIR=/opt/install/libairspy/include`

Please note that if you are using a recent version of libairspy (>= 1.0.6) the dynamic retrieval of sample rates is supported. To benefit from it you should modify the `plugins/samplesource/airspy/CMakeLists.txt` and change line `add_definitions(${QT_DEFINITIONS})` by `add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")`. In fact both lines are present with the last one commented out.

Be also aware that the lower rates (2.5 MS/s or 5 MS/s with modified firmware) are affected by a noise artifact so 10 MS/s is preferable for weak signal work or instrumentation. A decimation by 64 was implemented to facilitate narrow band work at 10 MS/s input rate.

<h2>Airspy HF+</h2>

[Airspy HF+](https://airspy.com/airspy-hf-plus/) is supported through [my fork of the airspyhf library](https://github.com/f4exb/airspyhf). This is needed because SDRangel uses integer samples at its input. The original library post processes the integer samples from the device and presents them as float samples without any other option. The maintainer of the original airspyhf library not being cooperative I had to keep changes on my own fork.

If you use your own location for libairspyhf install directory you need to specify library and include locations. Example with `/opt/install/libairspyhf` with the following defines on `cmake` command line:

`-DLIBAIRSPYHF_LIBRARIES=/opt/install/libairspyhf/lib/libairspyhf.so -DLIBAIRSPYHF_INCLUDE_DIR=/opt/install/libairspyhf/include`

It is recommended to add `-DRX_SAMPLE_24BIT` on the cmake command line to activate the Rx 24 bit DSP build and take advantage of improved dynamic range when using decimation.

<h2>BladeRF</h2>

[BladeRF](https://www.nuand.com/) is supported through the libbladerf library that should be installed in your system for proper build of the software and operation support. Add `libbladerf-dev` to the list of dependencies to install.

If you use your own location for libbladeRF install directory you need to specify library and include locations. Example with `/opt/install/libbladerf` with the following defines on `cmake` command line:

`-DLIBBLADERF_LIBRARIES=/opt/install/libbladeRF/lib/libbladeRF.so -DLIBBLADERF_INCLUDE_DIR=/opt/install/libbladeRF/include`

<h2>FunCube Dongle</h2>

Linux only.

Both Pro and Pro+ are supported with the plugins in fcdpro and fcdproplus respectively. For the Pro+ the band filter selection is not effective as it is handled by the firmware using the center frequency.

The control interface is based on qthid and has been built in the software in the fcdhid library. You don't need anything else than libusb support. Library fcdlib is used to store the constants for each dongle type.

The Pro+ has trouble starting. The sound card interface is not recognized when you just plug it in and start SDRAngel. The workaround is to start qthid then a recording program like Audacity and start recording in Audacity. Then just quit Audacity without saving and quit qthid. After this operation the Pro+ should be recognized by SDRAngel until you unplug it.

<h2>HackRF</h2>

[HackRF](https://greatscottgadgets.com/hackrf/) is supported through the libhackrf library that should be installed in your system for proper build of the software and operation support. Add `libhackrf-dev` to the list of dependencies to install. Please note that you will need a recent version (2015.07.2 at least) of libhackrf that supports the sequential listing of devices and the antenna power control (bias tee). To be safe anyway you may choose to build and install the Github version: `https://github.com/mossmann/hackrf.git`. Note also that the firmware must be updated to match the library version as per instructions found in the HackRF wiki.

If you use your own location for libhackrf install directory you need to specify library and include locations. Example with `/opt/install/libhackrf` with the following defines on `cmake` command line:

`-DLIBHACKRF_LIBRARIES=/opt/install/libhackrf/lib/libhackrf.so -DLIBHACKRF_INCLUDE_DIR=/opt/install/libhackrf/include`

HackRF is better used with a sampling rate of 4.8 MS/s and above. The 2.4 and 3.2 MS/s rates are considered experimental and are way out of specs of the ADC. You may or may not achieve acceptable results depending on the unit. A too low sampling rate will typically create ghost signals (images) and/or raise the noise floor.

<h2>LimeSDR</h2>

[LimeSDR](https://myriadrf.org/projects/limesdr/) and its smaller clone LimeSDR Mini are supported using LimeSuite library (see next).

<p><b>&#9888; The plugins should work normally when running as single instances. Support of many Rx and/or Tx instances running concurrently is considered experimental. At least you should always have one of the streams running.</b></p>

<p>&#9888; It seems LimeSDR mini has trouble working with host sample rates lower than 2.5 MS/s particularly in Tx mode.</p>

You will need a minimal installation of LimeSuite. Presently commit 04b57e0 or later should be used with its corresponding firmware (v4) and gateware (v2.12) installed in the LimeSDR device:

  - `sudo apt-get install libsqlite3-dev`
  - `git clone https://github.com/myriadrf/LimeSuite.git`
  - `cd LimeSuite`
  - `mkdir builddir`
  - `cd builddir`
  - `cmake -DCMAKE_INSTALL_PREFIX=/opt/install/LimeSuite ..`
  - `make -j8`
  - `make install`

Then add the following defines on `cmake` command line:

`-DLIMESUITE_INCLUDE_DIR=/opt/install/LimeSuite/include -DLIMESUITE_LIBRARY=/opt/install/LimeSuite/lib/libLimeSuite.so`

<h2>PlutoSDR</h2>

[PlutoSDR](https://wiki.analog.com/university/tools/pluto) is supported with the libiio interface. This library should be installed in your system for proper build of the software and operation support. Add `libiio-dev` to the list of dependencies to install. Be aware that version 0.10 is needed and is not available yet in all distributions. You may have to compile it from source instead.

If you use your own location for libiio install directory you need to specify library and include locations. Example with `/opt/install/libiio` with the following defines on `cmake` command line: `-DLIBIIO_INCLUDE_DIR=/opt/install/libiio/include -DLIBIIO_LIBRARY=/opt/install/libiio/lib/libiio.so`. In openSuse the lib directory path would be: `-DLIBIIO_LIBRARY=/opt/install/libiio/lib64/libiio.so`.

<h2>RTL-SDR</h2>

RTL-SDR based dongles are supported through the librtlsdr library that should be installed in your system for proper build of the software and operation support. Add `librtlsdr-dev` to the list of dependencies to install.

If you use your own location for librtlsdr install directory you need to specify library and include locations. Example with `/opt/install/librtlsdr` with the following defines on `cmake` command line:

`-DLIBRTLSDR_LIBRARIES=/opt/install/librtlsdr/lib/librtlsdr.so -DLIBRTLSDR_INCLUDE_DIR=/opt/install/librtlsdr/include`

<h2>SDRplay RSP1</h2>

Linux only.

[SDRplay RSP1](https://www.sdrplay.com/rsp1/) is supported through the [libmirisdr-4](https://github.com/f4exb/libmirisdr-4) library found in this very same Github space. There is no package distribution for this library and you will have to clone it, build and install it in your system. However Debian packages of SDRangel contain a pre-compiled version of this library.

If you use your own location for libmirisdr-4 install directory you need to specify library and include locations with cmake. For example with `/opt/install/libmirisdr` the following defines must be added on `cmake` command line:

`-DLIBMIRISDR_LIBRARIES=/opt/install/libmirisdr/lib/libmirisdr.so -DLIBMIRISDR_INCLUDE_DIR=/opt/install/libmirisdr/include`

&#9888; The RSP1 has been discontinued in favor of RSP1A. Unfortunately due to their closed source nature RSP1A nor RSP2 can be supported in SDRangel.

<h1>Plugins for special devices</h1>

These plugins do not use any hardware device connected to your system.

<h2>File input</h2>

The [File source plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/filesource) allows the playback of a recorded IQ file. Such a file is obtained using the recording feature. Click on the record button on the left of the main frequency dial to toggle recording. The file has a fixed name `test_<n>.sdriq` created in the current directory where `<n>` is the device tab index.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is alwasys available in the list of devices as `FileSource[0]` even if no physical device is connected.

The `.sdriq` format produced are the 2x2 bytes I/Q samples with a header containing the center frequency of the baseband, the sample rate and the timestamp of the recording start. Note that this header length is a multiple of the sample size so the file can be read with a simple 2x2 bytes I/Q reader such as a GNU Radio file source block. It will just produce a short glitch at the beginning corresponding to the header data. 

<h2>File output</h2>

The [File sink plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/filesink) allows the recording of the I/Q baseband signal produced by a transmission chain to a file in the `.sdriq` format thus readable by the file source plugin described just above.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `FileSink[0]` even if no physical device is connected.

<h2>Test source</h2>

The [Test source plugin](https://github.com/f4exb/sdrangel/tree/master/plugins/samplesource/testsource) is an internal continuous wave generator that can be used to carry out test of software internals. 

<h2>SDRdaemon receiver input</h2>

Linux only.

The [SDRdaemon source input plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/sdrdaemonsource) is the client side of the SDRdaemon receiver server `sdrdaemonrx`. See the [SDRdaemon](https://github.com/f4exb/sdrdaemon) project in this Github repository. You must specify the local address and UDP port to which the remote server connects and samples will flow into the SDRangel application (default is `127.0.0.1`port `9090`). It uses the meta data to retrieve the sample flow characteristics such as sample rate and receiveng center frequency. It also opens a TCP link to another port to send service messages such as setting parameters specific to the hadrware device connected to the server (default port is `9091`). The `libnanomsg` library is used to support this messaging.

The data blocks transmitted via UDP are protected against loss with a Cauchy MDS block erasure codec. This makes the transmission more robust in particular with WiFi links.

There is an automated skew rate compensation in place. During rate readjustemnt streaming can be suspended or signal glitches can occur for about one second.

This plugin will be built only if the libnanomsg and the [CM256cc library](https://github.com/f4exb/cm256cc) are installed in your system. libnanomsg is available as a dev package in most distributions For CM256cc if you install it in a non standard directory you will then have to specify the include and library paths on the cmake command line. Say if you install cm256cc in `/opt/install/cm256cc` you will have to add `-DCM256CC_INCLUDE_DIR=/opt/install/cm256cc/include/cm256cc -DCM256CC_LIBRARIES=/opt/install/cm256cc/lib/libcm256cc.so` to the cmake commands.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `SDRdaemonSource[0]` even if no physical device is connected.

<h2>SDRdaemon transmitter output</h2>

Linux only.

The [SDRdaemon sink output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/sdrdaemonsink) is the client side of the SDRdaemon transmitter server `sdrdaemontx`. See the [SDRdaemon](https://github.com/f4exb/sdrdaemon) project in this Github repository. You must specify the distant address and UDP port to which the plugin connects and samples from the SDRangel application will flow into the transmitter server (default is `127.0.0.1`port `9092`). It also opens a TCP link to another port to exchange service messages such as setting the center frequency or getting status information from the server (default port is `9093`). The `libnanomsg` library is used to support this messaging.

The data blocks sent via UDP are protected against loss with a Cauchy MDS block erasure codec. This makes the transmission more robust in particular with WiFi links.

There is an automated skew rate compensation in place so that the generator throttling is adjusted to match the actual sample rate of the distant device. This is based on the number of buffer blocks sent back from the distant server using the TCP link. 

This plugin will be built only if the libnanomsg and the [CM256cc library](https://github.com/f4exb/cm256cc) are installed in your system. libnanomsg is available as a dev package in most distributions For CM256cc if you install it in a non standard directory you will then have to specify the include and library paths on the cmake command line. Say if you install cm256cc in `/opt/install/cm256cc` you will have to add `-DCM256CC_INCLUDE_DIR=/opt/install/cm256cc/include/cm256cc -DCM256CC_LIBRARIES=/opt/install/cm256cc/lib/libcm256cc.so` to the cmake commands.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `SDRdaemonSink[0]` even if no physical device is connected.

<h1>Channel plugins with special conditions</h1>

<h2>DSD (Digital Speech Decoder)</h2>

This is the `demoddsd` plugin. At present it can be used to decode the following digital speech formats:

  - DMR/MOTOTRBO
  - dPMR
  - D-Star
  - Yaesu System Fusion (YSF)

It is based on the [DSDcc](https://github.com/f4exb/dsdcc) C++ library which is a rewrite of the original [DSD](https://github.com/szechyjs/dsd) program. So you will need to have DSDcc installed in your system. Please follow instructions in [DSDcc readme](https://github.com/f4exb/dsdcc/blob/master/Readme.md) to build and install DSDcc. If you install it in a custom location say `/opt/install/dsdcc` you will need to add these defines to the cmake command: `-DLIBDSDCC_INCLUDE_DIR=/opt/install/dsdcc/include/dsdcc -DLIBDSDCC_LIBRARIES=/opt/install/dsdcc/lib/libdsdcc.so`

If you have one or more serial devices interfacing the AMBE3000 chip in packet mode you can use them to decode AMBE voice frames. For that purpose you will need to compile with [SerialDV](https://github.com/f4exb/serialDV) support. Please refer to this project Readme.md to compile and install SerialDV. If you install it in a custom location say `/opt/install/serialdv` you will need to add these defines to the cmake command: `-DLIBSERIALDV_INCLUDE_DIR=/opt/install/serialdv/include/serialdv -DLIBSERIALDV_LIBRARY=/opt/install/serialdv/lib/libserialdv.so` Also your user must be a member of group `dialout` to be able to use the dongle.

Although such serial devices work with a serial interface at 400 kb in practice maybe for other reasons they are capable of handling only one conversation at a time. The software will allocate the device dynamically to a conversation with an inactivity timeout of 1 second so that conversations do not get interrupted constantly making the audio output too choppy. In practice you will have to have as many devices connected to your system as the number of conversations you would like to be handled in parallel. 

Note that this is not supported in Windows because of trouble with COM port support (contributors welcome!).

---
&#9888; Since kernel 4.4.52 the default for FTDI devices (that is in the ftdi_sio kernel module) is not to set it as low latency. This results in the ThumbDV dongle not working anymore because its response is too slow to sustain the normal AMBE packets flow. The solution is to force low latency by changing the variable for your device (ex: /dev/ttyUSB0) as follows:

`echo 1 | sudo tee /sys/bus/usb-serial/devices/ttyUSB0/latency_timer` or `sudo setserial /dev/ttyUSB0 low_latency`

---

Alternatively you can use [mbelib](https://github.com/szechyjs/mbelib) but mbelib comes with some copyright issues (see next). If you have mbelib installed in a custom location, say `/opt/install/mbelib` you will need to add these defines to the cmake command: `-DLIBMBE_INCLUDE_DIR=/opt/install/mbelib/include -DLIBMBE_LIBRARY=/opt/install/mbelib/lib/libmbe.so`

Possible copyright issues apart (see next) the audio quality with the DVSI AMBE chip is much better.

While DSDcc is intended to be patent-free, `mbelib` that it uses describes functions that may be covered by one or more U.S. patents owned by DVSI Inc. The source code itself should not be infringing as it merely describes possible methods of implementation. Compiling or using `mbelib` may infringe on patents rights in your jurisdiction and/or require licensing. It is unknown if DVSI will sell licenses for software that uses `mbelib`.

Possible copyright issues apart the audio quality with the DVSI AMBE chip is much better.

If you are not comfortable with this just do not install DSDcc and/or mbelib and the plugin will not be compiled and added to SDRangel. For packaged distributions just remove from the installation directory:

  - For Linux distributions: `plugins/channel/libdemoddsd.so`
  - For Windows distributions: `dsdcc.dll`, `mbelib.dll`, `plugins\channel\demoddsd.dll`

<h1>Software distributions</h1>

In the [releases](https://github.com/f4exb/sdrangel/releases) section one can find binary distributions for some common systems:

  - Debian x86_64 (Ubuntu 16.04, Ubuntu 17.04, Ubuntu 17.10, Debian Stretch)
  - Windows 32 bit (runs also in 64 bit Windows) 
  - Windows 64 bit

<h2>Debian distributions</h2>

It is provided in the form of .deb packages for x86_64 architectures with SSE 4.1 support. 

Install it as usual for .deb packages:

  - Make sure the `universe` repository is in your `/etc/apt/sources.list`

Prior to apt-get v 1.1 (before Ubuntu 16.04) in a terminal do:

  - `sudo apt-get update`
  - `sudo apt-get upgrade`
  - `sudo dpkg -i sdrangel_vx.y.z-1_amd64.deb` where x.y.z is the version number
  - `sudo apt-get -f install` this will install missing dependencies
    
Since apt-get v 1.1 installation is possible from a local file:

  - cd to where the archive has been downloaded
  - `sudo apt-get install ./sdrangel_vx.y.z-1_amd64.deb` where x.y.z is the version number 
    
The software is installed in `/opt/sdrangel` you can start it from the command line with:
  - `/opt/sdrangel/bin/sdrangel`
  
<h2>Windows distributions</h2>

This is the archive of the complete binary distribution that expands to the `sdrangel64` directory for the 64 bit version and `sdrangel` for the 32 bit version. You can install it anywhere you like and click on `sdrangel.exe` to start.

<b>&#9888; Windows distributions are provided as by products thanks to the Qt toolchain. The platform of choice to run SDRangel is definitely Linux and very little support can be given for the Windows distributions.</b>

<h1>Software build</h1>

<h2>Qt version</h2>

To be sure you will need at least Qt version 5.5. It definitely does not work with versions earlier than 5.3 but neither 5.3 nor 5.4 were tested.

  - Linux builds are made with 5.5.1 (Xenial), 5.7 (Zesty) and 5.9 (Artful)
  - Windows 32 build is made with 5.9.1 and has Qt ANGLE support (OpenGL emulation with DirectX)
  - Windows 64 build is made with 5.9.1 and has no Qt ANGLE support (native OpenGL)

<h2>24 bit DSP</h2>

By default all Rx DSP processes use 16 bit samples coded on int16 fields. In order to use 24 bit samples coded on int32 fields you can specify `-DRX_SAMPLE_24BIT` on the cmake command line. This will give more dynamic range when the number of bits with decimation exceeds 16 bits:

  - RTL-SDR, HackRF: (8 bit native) no advantage
  - Funcube Pro and Pro+: (16 bit native) no decimation hence no advantage
  - Airspy, BladeRF, LimeSDR, PlutoSDR, SDRPlay: (12 bit native) advantage for decimation by 32 or 64
  - AirspyHF: (16 bit native) advantage for any decimation

<h2>Ubuntu</h2>

<h3>Prerequisites for 14.04 LTS</h3>

Prerequisite to install Qt5 libraries properly:
`sudo apt-get install libgles2-mesa-dev`

Install cmake version 3:

  - `sudo apt-get install software-properties-common`
  - `sudo add-apt-repository ppa:george-edison55/cmake-3.x`
  - `sudo apt-get update`
  - `sudo apt-get remove cmake` (if already installed)
  - `sudo apt-get install cmake`

<h3>With newer versions just do:</h3>

  - `sudo apt-get install cmake g++ pkg-config libfftw3-dev libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev libboost-all-dev libasound2-dev pulseaudio libnanomsg-dev libopencv-dev libsqlite3-dev libxml2-dev`
  - `mkdir build && cd build && cmake ../ && make`

`librtlsdr-dev` is in the `universe` repo. (utopic 14.10 amd64.)

<h2>Mint</h2>

Tested with Cinnamon 17.2. Since it is based on Ubuntu 14.04 LTS pleae follow instructions for this distribution (paragraph just above).

<h2>Debian</h2>

For any version of Debian you will need Qt5.

Debian 7 "wheezy" uses Qt4. Qt5 is available from the "wheezy-backports" repo, but this will remove Qt4. Debian 8 "jessie" uses Qt5.

For Debian Jessie or Stretch:

`sudo apt-get install cmake g++ pkg-config libfftw3-dev libusb-1.0-0-dev libusb-dev qt5-default qtbase5-dev qtchooser libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev librtlsdr-dev libboost-all-dev libasound2-dev pulseaudio libopencv-dev libsqlite3-dev`

`mkdir build && cd build && cmake ../ && make`

<h2>openSUSE</h2>

This has been tested with the bleeding edge "Thumbleweed" distribution:

`sudo zypper install Mesa-libGL1 Mesa-libEGL-devel Mesa-libGL-devel Mesa-libGLESv1_CM-devel Mesa-libGLESv2-devel Mesa-libGLESv3-devel Mesa-libglapi-devel libOSMesa-devel` 

`sudo zypper install cmake fftw3-devel gcc-c++ libusb-1_0-devel libqt5-qtbase-devel libQt5OpenGL-devel libqt5-qtmultimedia-devel libqt5-qttools-devel libQt5Network-devel libQt5Widgets-devel boost-devel alsa-devel pulseaudio opencv-devel`

Then you should be all set to build the software with `cmake` and `make` as discussed earlier.

  - Note1: if you are on Leap you will need a more recent g++ compiler so in place of `gcc-c++` use `gcc5-c++` or `gcc6-c++` then add the following in the cmake command: `-DCMAKE_C_COMPILER=/usr/bin/gcc-6 -DCMAKE_CXX_COMPILER=/usr/bin/g++-6` (for gcc 6) and then `-DCMAKE_INSTALL_PREFIX:PATH=...` for the custom install path (not `-DCMAKE_INSTALL_PREFIX=...`)
  - Note2: On Leap and aarch64 architectures you will need to build and install `libnanomsg` from [source](https://github.com/nanomsg/nanomsg)
  - Note3 for udev rules: installed udev rules for BladeRF and HackRF are targetted at Debian or Ubuntu systems that have a plugdev group for USB hotplug devices. This is not the case in openSUSE. To make the udev rules file compatible just remove the `GROUP` parameter on all lines and change `MODE` parameter to `666`.
  - Note4: A package has been created in OpenSUSE thanks to Martin, see: [sdrangel](https://build.opensuse.org/package/show/hardware:sdr/sdrangel). It is based on the latest release on master branch.

<h2>Fedora</h2>

This has been tested with Fedora 23 and 22:

  - `sudo dnf groupinstall "C Development Tools and Libraries"`
  - `sudo dnf install mesa-libGL-devel`
  - `sudo dnf install cmake gcc-c++ pkgconfig fftw-devel libusb-devel qt5-qtbase-devel qt5-qtmultimedia-devel qt5-qttools-devel boost-devel pulseaudio alsa-lib-devel`

Then you should be all set to build the software with `cmake` and `make` as discussed earlier.

  - Note for udev rules: the same as for openSUSE applies. This is detailed in the previous paragraph for openSUSE.

<h2>Manjaro</h2>

Tested with the 15.09 version with LXDE desktop (community supported). The exact desktop environment should not matter anyway. Since Manjaro is Arch Linux based prerequisites should be similar for Arch and all derivatives.

`sudo pacman -S cmake pkg-config fftw qt5-multimedia qt5-tools qt5-base libusb boost boost-libs pulseaudio`

Then you should be all set to build the software with `cmake` and `make` as discussed earlier.

  - Note1 for udev rules: the same as for openSUSE and Fedora applies.
  - Note2: A package has been created in the AUR (thanks Mikos!), see: [sdrangel-git](https://aur.archlinux.org/packages/sdrangel-git). It is based on the `205fee6` commit of 8th December 2015.

<h2>Windows</h2>

This is a rather long story and one may prefer using the software distribution instead. However the brave may follow [this link](ReadmeWindowsBuild.md)

<h1>Mac O/S</h1>

A Mac O/S build was contributed from version 2.0.1. Please be aware that this is still experimental.

<h1>Android</h1>

Despite several attempts and the presence of Android related stuff still present in the .pro files there is no Android build available. An APK can be built but Qt fails miserably at porting applications other than its ridiculously simple examples. When multi-threading is involved a lot like in SDRangel this simply crashes at the very beginning of the application when starting the event loop.

Contributors welcome!


<h1>Software installation on Linux flavours</h1>

Simply do `make install` or `sudo make install` depending on you user rights on the target installation directory. On most systems the default installation directory is `/usr/local` a custom installation directory can be specified with the `-DCMAKE_INSTALL_PREFIX=...` option on the `cmake` command line as usual with cmake.

You can uninstall the software with `make uninstall` or `sudo make uninstall` from the build directory (it needs the `install_manifest.txt` file in the same directory and is automatically created by the `make install`command). Note that this will not remove the possible empty directories.

<h1>Known Issues</h1>

  - The message queuing model supports a n:1 connection to an object (on its input queue) and a 1:1 connection from an object (on its output queue). Assuming a different model can cause insidious disruptions.
  - As the objects input and output queues can be publicly accessed there is no strict control of which objects post messages on these queues. The correct assumption is that messages can be popped from the input queue only by its holder and that messages can be pushed on the output queue only by its holder.
  - Objects managing more than one message queue (input + output for example) do not work well under stress conditions. Output queue removed from sample sources but this model has to be revised throughout the application.
  - SDRdaemon FEC plugin: it has trouble doing the first connection or reconnecting to another device. The best option is to try then acknowledge the error message and restart SDRangel.

<h1>Limitations</h1>

  - Your hardware. Still SDRangel is relatively conservative on computer resources.
  - OpenGL 3+ (Linux) and 4.3+ (Windows) is required

<h1>Features</h1>

<h2>Changes from SDRangelove</h2>

See the v1.0.1 first official relase [release notes](https://github.com/f4exb/sdrangel/releases/tag/v1.0.1)

<h2>To Do</h2>

Since version 3.3.2 the "todos" are in the form of tickets opened in the Issues section with the label "feature". When a specific release is targeted it will appear as a milestone. Thus anyone can open a "feature" issue to request a new feature.

Other ideas:

  - Enhance presets management (Edit, Move, Import/Export from/to human readable format like JSON).
  - Allow arbitrary sample rate for channelizers and demodulators (not multiple of 48 kHz). Prerequisite for polyphase channelizer
  - Implement polyphase channelizer
  - Level calibration
  - Even more demods. Contributors welcome!

<h1>Developer's notes</h1>

Please check the developper's specific [readme](./ReadmeDeveloper.md)
