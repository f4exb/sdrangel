![SDR Angel banner](doc/img/sdrangel_banner.png)

**SDRangel** is an Open Source Qt5 / OpenGL 3.0+ SDR and signal analyzer frontend to various hardware.

**Check the discussion group** [here](https://groups.io/g/sdrangel)

&#9888; SDRangel is intended for the power user. We expect you to already have some experience with SDR applications and digital signal processing in general. SDRangel might be a bit overwhelming for you however you are encouraged to use the discussion group above to look for help. You can also find more information in the [Wiki](https://github.com/f4exb/sdrangel/wiki).

<h1>Hardware requirements</h1>

SDRangel is a near real time application that is demanding on CPU power and clock speeds for low latency. Recent (2015 or later) Core i7 class CPU is recommended preferably with 4 HT CPU cores (8 logical CPUs or more) with nominal clock over 2 GHz and at least 8 GB RAM. Modern Intel processors will include a GPU suitable for proper OpenGL support. On the other hand SDRangel is not as demanding as recent computer games for graphics and CPU integrated graphics are perfectly fine. USB-3 ports are also preferable for high speed, low latency USB communication. It may run on beefy SBCs and was tested successfully on a Udoo Ultra.

The server variant does not need a graphical display and therefore OpenGL support. It can run on smaller devices check the Wiki for hardware requirements and a list of successful cases.

<h1>Source code</h1>

<h2>Repository branches</h2>

- master: the production branch
- dev: the development branch
- legacy: the modified code from the parent application [hexameron rtl-sdrangelove](https://github.com/hexameron/rtl-sdrangelove) before a major redesign of the code was carried out and sync was lost.

<h2>Untested plugins</h2>

These plugins come from the parent code base and have been maintained so that they compile but they are not being actively tested:

channelrx:

  - demodlora

<h1>Specific features</h1>

<h2>Multiple device support</h2>

Since version 2 SDRangel can integrate more than one hardware device running concurrently.

<h2>Transmission support</h2>

Since version 3 transmission or signal generation is supported for BladeRF, HackRF (since version 3.1), LimeSDR (since version 3.4) and PlutoSDR (since version 3.7.8) using a sample sink plugin. These plugins are:

  - [BladeRF1 output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/bladerf1output)
  - [BladeRF2 output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/bladerf2output)
  - [HackRF output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/hackrfoutput)
  - [LimeSDR output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/limesdroutput)
  - [PlutoSDR output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/plutosdroutput)
  - [XTRX output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/xtrxoutput) Experimental.
  - [File output or file sink plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/filesink)
  - [Remote device via Network](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/remoteoutput) Linux only

<h2>REST API</h2>

Since version 4 a REST API is available to interact with the SDRangel application. More details are provided in the server instance documentation in the `sdrsrv` folder.

<h2>Server instance</h2>

Since version 4 the `sdrangelsrv` binary launches a server mode SDRangel instance that runs wihout the GUI. More information is provided in the Readme file of the `sdrsrv` folder.

<h2>Detached RF head server</h2>

Since version 4.1 the previously separated project SDRdaemon has been modified and included in SDRangel. The `sdrangelsrv` headless variant can be used for this purpose using the Remote source or sink channels.

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

<h1>Software build on Linux</h1>

Plese consult the [Wiki page for compilation in Linux](https://github.com/f4exb/sdrangel/wiki/Compile-from-source-in-Linux). The notes below are left for further information if needed although you should be all set with the Wiki.

<h2>Qt version</h2>

To be sure you will need at least Qt version 5.5. It definitely does not work with versions earlier than 5.3 but neither 5.3 nor 5.4 were tested.

  - Linux builds are made with 5.5.1 (Xenial), 5.9 (Artful, Stretch, Bionic) and 5.11 (Cosmic)
  - Windows build is made with 5.10.1 and has Qt ANGLE support (OpenGL emulation with DirectX)

&#9758; From version 3.12.0 the Linux binaries are built with the 24 bit Rx option.

<h2>Ubuntu</h2>

  - `sudo apt-get install cmake g++ pkg-config libfftw3-dev libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev libboost-all-dev libasound2-dev pulseaudio libopencv-dev libxml2-dev bison flex ffmpeg libavcodec-dev libavformat-dev libopus-dev`

<h2>Debian</h2>

  - `sudo apt-get install cmake g++ pkg-config libfftw3-dev libusb-1.0-0-dev libusb-dev qt5-default qtbase5-dev qtchooser libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev librtlsdr-dev libboost-all-dev libasound2-dev pulseaudio libopencv-dev  libxml2-dev bison flex ffmpeg libavcodec-dev libavformat-dev libopus-dev`

<h2>openSUSE</h2>

  - `sudo zypper install Mesa-libGL1 Mesa-libEGL-devel Mesa-libGL-devel Mesa-libGLESv1_CM-devel Mesa-libGLESv2-devel Mesa-libGLESv3-devel Mesa-libglapi-devel libOSMesa-devel`
  - `sudo zypper install cmake fftw3-devel gcc-c++ libusb-1_0-devel libqt5-qtbase-devel libQt5OpenGL-devel libqt5-qtmultimedia-devel libqt5-qttools-devel libQt5Network-devel libQt5Widgets-devel boost-devel alsa-devel pulseaudio opencv-devel`

  - Note1: if you are on Leap you will need a more recent g++ compiler so in place of `gcc-c++` use `gcc6-c++` or `gcc7-c++` then add the following in the cmake command: `-DCMAKE_C_COMPILER=/usr/bin/gcc-7 -DCMAKE_CXX_COMPILER=/usr/bin/g++-7` (for gcc 7) and then `-DCMAKE_INSTALL_PREFIX:PATH=...` for the custom install path (not `-DCMAKE_INSTALL_PREFIX=...`)
  - Note2: A package has been created in openSUSE thanks to Martin, see: [sdrangel](https://build.opensuse.org/package/show/hardware:sdr/sdrangel). It is based on the latest release on master branch.

<h2>Fedora</h2>

This has been tested with Fedora 23 and 22:

  - `sudo dnf groupinstall "C Development Tools and Libraries"`
  - `sudo dnf install mesa-libGL-devel`
  - `sudo dnf install cmake gcc-c++ pkgconfig fftw-devel libusb-devel qt5-qtbase-devel qt5-qtmultimedia-devel qt5-qttools-devel boost-devel pulseaudio alsa-lib-devel`

<h2>Arch Linux / Manjaro</h2>

Tested with the 15.09 version with LXDE desktop (community supported). The exact desktop environment should not matter anyway. Prerequisites should be similar for Arch and all derivatives.

`sudo pacman -S cmake pkg-config fftw qt5-multimedia qt5-tools qt5-base libusb boost boost-libs pulseaudio`

  - Note1: Two package are avaliable in the AUR (thanks Mikos!), [sdrangel](https://aur.archlinux.org/packages/sdrangel), which provides the lastest tagged release (stable), and [sdrangel-git](https://aur.archlinux.org/packages/sdrangel-git), which builds the latest commit from this repository (unstable).

<h1>Compile for Windows</h1>

This is a rather long story and one may prefer using the software distribution instead. However the brave may follow [this link](ReadmeWindowsBuild.md)

<h1>Compile for Mac O/S</h1>

A Mac O/S build was contributed from version 2.0.1. Please be aware that this is still experimental. In the [SDRangel discussion group](https://groups.io/g/sdrangel) Mac O/S users have discussed the matter extensively and produced builds. It could be a good idea to check it.

<h1>Compile for Android</h1>

Despite several attempts and the presence of Android related stuff still present in the .pro files there is no Android build available. An APK can be built but Qt fails miserably at porting applications other than its ridiculously simple examples. When multi-threading is involved a lot like in SDRangel this simply crashes at the very beginning of the application when starting the event loop.

Contributors welcome!

<h1>Supported hardware</h1>

&#9758; Details on how to compile support libraries and integrate them in the final build of SDRangel are detailed in the [Wiki page for compilation in Linux](https://github.com/f4exb/sdrangel/wiki/Compile-from-source-in-Linux) mentioned earlier.

The notes below are left for complementary information

<h2>Airspy</h2>

[Airspy R2](https://airspy.com/airspy-r2/) and [Airspy Mini](https://airspy.com/airspy-mini/) are supported through the libairspy library that should be installed in your system for proper build of the software and operation support.

Please note that if you are using a recent version of libairspy (>= 1.0.6) the dynamic retrieval of sample rates is supported. To benefit from it you should modify the `plugins/samplesource/airspy/CMakeLists.txt` and change line `add_definitions(${QT_DEFINITIONS})` by `add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")`. In fact both lines are present with the last one commented out.

Be also aware that the lower rates (2.5 MS/s or 5 MS/s with modified firmware) are affected by a noise artifact so 10 MS/s is preferable for weak signal work or instrumentation. A decimation by 64 was implemented to facilitate narrow band work at 10 MS/s input rate.

<h2>Airspy HF+</h2>

[Airspy HF+](https://airspy.com/airspy-hf-plus/) is supported through [the airspyhf library](https://github.com/airspy/airspyhf).

It is recommended to add `-DRX_SAMPLE_24BIT=ON` on the cmake command line to activate the Rx 24 bit DSP build and take advantage of improved dynamic range when using decimation.

&#9758; From version 3.12.0 the Linux binaries are built with the 24 bit Rx option.

<h2>BladeRF classic (v.1)</h2>

[BladeRF1](https://www.nuand.com/bladerf-1) is supported through the libbladeRF library. Note that libbladeRF v2 is used since version 4.2.0 (git tag 2018.08).

&#9758; Please note that if you use your own library the FPGA image `hostedx40.rbf` or `hostedx115.rbf` shoud be placed in e.g. `/opt/install/libbladeRF/share/Nuand/bladeRF` unless you have flashed the FPGA image inside the BladeRF.

The plugins used to support BladeRF classic are specific to this version of the BladeRF:
  - [BladeRF1 input](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/bladerf1input)
  - [BladeRF1 output](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/bladerf1output)

<h2>BladeRF micro (v.2)</h2>

From version 4.2.0.

[BladeRF 2 micro](https://www.nuand.com/bladerf-2-0-micro/) is also supported using libbladeRF library.

The plugins used to support BladeRF 2 micro are specific to this version of the BladeRF:
  - [BladeRF2 input](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/bladerf2input)
  - [BladeRF2 output](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/bladerf2output)

<h2>FunCube Dongle</h2>

Linux only.

Both Pro and Pro+ are supported with the plugins in fcdpro and fcdproplus respectively. For the Pro+ the band filter selection is not effective as it is handled by the firmware using the center frequency.

The control interface is based on qthid and has been built in the software in the fcdhid library. You don't need anything else than libusb support. Library fcdlib is used to store the constants for each dongle type.

The Pro+ has trouble starting. The sound card interface is not recognized when you just plug it in and start SDRAngel. The workaround is to start qthid then a recording program like Audacity and start recording in Audacity. Then just quit Audacity without saving and quit qthid. After this operation the Pro+ should be recognized by SDRAngel until you unplug it.

<h2>HackRF</h2>

[HackRF](https://greatscottgadgets.com/hackrf/) is supported through the libhackrf library. Please note that you will need a recent version (2015.07.2 at least) of libhackrf that supports the sequential listing of devices and the antenna power control (bias tee). To be safe anyway you may choose to build and install the Github version: `https://github.com/mossmann/hackrf.git`. Note also that the firmware must be updated to match the library version as per instructions found in the HackRF wiki.

HackRF is better used with a sampling rate of 4.8 MS/s and above. The 2.4 and 3.2 MS/s rates are considered experimental and are way out of specs of the ADC. You may or may not achieve acceptable results depending on the unit. A too low sampling rate will typically create ghost signals (images) and/or raise the noise floor.

<h2>LimeSDR</h2>

[LimeSDR](https://myriadrf.org/projects/limesdr/) and its smaller clone LimeSDR Mini are supported using LimeSuite library (see next).

<p>&#9888; Version 18.10.0 of LimeSuite is used in the builds and corresponding gateware loaded in the LimeSDR should be is used. If you compile from source version 18.10.0 of LimeSuite must be used.</p>

<p>&#9888; LimeSDR-Mini seems to have problems with Zadig driver therefore it is supported in Linux only. A workaround is to use it through SoapySDR</p>

<h2>Perseus</h2>

Linux only.

[The Perseus](http://microtelecom.it/perseus/) is supported with [my fork of libperseus-sdr library](https://github.com/f4exb/libperseus-sdr.git) on the `fixes` branch which is the default. There are a few fixes from the original mainly to make it work in a multi-device context.

Please note that the Perseus input plugin will be built only if the 24 bit Rx DSP chain is activated in the compilation with the `-DRX_SAMPLE_24BIT=ON` option on the cmake command line.

&#9758; From version 3.12.0 the Linux binaries are built with the 24 bit Rx option and Perseus input plugin.

<h2>PlutoSDR</h2>

[PlutoSDR](https://wiki.analog.com/university/tools/pluto) is supported with the libiio interface. Be aware that version 0.16 or higher is needed. Also the device should be flashed with firmware 0.29 or higher.

<h2>RTL-SDR</h2>

RTL-SDR based dongles are supported through the librtlsdr library.

<h2>SDRplay RSP1</h2>

Linux only.

[SDRplay RSP1](https://www.sdrplay.com/rsp1/) is supported through the [libmirisdr-4](https://github.com/f4exb/libmirisdr-4) library found in this very same Github space. There is no package distribution for this library and you will have to clone it, build and install it in your system. However Debian packages of SDRangel contain a pre-compiled version of this library.

&#9888; The RSP1 has been discontinued in favor of RSP1A. Unfortunately due to their closed source nature RSP1A nor RSP2 can be supported natively in SDRangel. As a workaround you may use it through SoapySDR.

<h2>XTRX</h2>

Experimental and Linux only. Compile from source.

[XTRX](https://xtrx.io) is supported through the set of [xtrx libraries](https://github.com/xtrx-sdr/images).

Before starting SDRangel you have to add the library directory to the `LD_LIBRARY_PATH` variable for example with `export LD_LIBRARY_PATH=/opt/install/xtrx-images/lib:$LD_LIBRARY_PATH`.

&#9888; Reception may stall sometimes particularly with sample rates lower than 5 MS/s and also after changes. You may need to stop and restart the device (stop/start button) to recover.

&#9888; Right after (re)start you may need to move the main frequency dial back and forth if you notice that you are not on the right frequency.

&#9888; Simultaneous Rx and Tx is not supported. Dual Tx is not working either.

<h1>Plugins for special devices</h1>

These plugins do not use any hardware device connected to your system.

<h2>File input</h2>

The [File source plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/filesource) allows the playback of a recorded IQ file. Such a file is obtained using the recording feature. Click on the record button on the left of the main frequency dial to toggle recording. The file has a fixed name `test_<n>.sdriq` created in the current directory where `<n>` is the device tab index.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `FileSource[0]` even if no physical device is connected.

The `.sdriq` format produced are the 2x2 bytes I/Q samples with a header containing the center frequency of the baseband, the sample rate and the timestamp of the recording start. Note that this header length is a multiple of the sample size so the file can be read with a simple 2x2 bytes I/Q reader such as a GNU Radio file source block. It will just produce a short glitch at the beginning corresponding to the header data.

<h2>File output</h2>

The [File sink plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/filesink) allows the recording of the I/Q baseband signal produced by a transmission chain to a file in the `.sdriq` format thus readable by the file source plugin described just above.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `FileSink[0]` even if no physical device is connected.

<h2>Test source</h2>

The [Test source plugin](https://github.com/f4exb/sdrangel/tree/master/plugins/samplesource/testsource) is an internal continuous wave generator that can be used to carry out test of software internals.

<h2>Remote input</h2>

Linux only.

The [Remote input plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/remoteinput) is the client side of an instance of SDRangel running the Remote Sink channel plugin. On the "Data" line you must specify the local address and UDP port to which the remote server connects and samples will flow into the SDRangel application (default is `127.0.0.1`port `9090`). It uses the meta data to retrieve the sample flow characteristics such as sample rate and receiving center frequency. The remote is entirely controlled by the REST API. On the "API" line you can specify the address and port at which the remote REST API listens. However it is used just to display basic information about the remote.

The data blocks transmitted via UDP are protected against loss with a Cauchy MDS block erasure codec. This makes the transmission more robust in particular with WiFi links.

There is an automated skew rate compensation in place. During rate readjustment streaming can be suspended or signal glitches can occur for about one second.

This plugin will be built only if the [CM256cc library](https://github.com/f4exb/cm256cc) is installed in your system.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `RemoteInput` even if no physical device is connected.

<h2>Remote output</h2>

Linux only.

The [Remote output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/remoteoutput) is the client side of and instance of SDRangel running the Remote Source channel plugin. On the "Data" line you must specify the distant address and UDP port to which the plugin connects and samples from the SDRangel application will flow into the transmitter server (default is `127.0.0.1`port `9090`). The remote is entirely controlled by the REST API. On the "API" line you can specify the address and port at which the remote REST API listens. The API is pinged regularly to retrieve the status of the data blocks queue and allow rate control to stabilize the queue length. Therefore it is important to connect to the API properly (The status line must return "API OK" and the API label should be lit in green).

The data blocks sent via UDP are protected against loss with a Cauchy MDS block erasure codec. This makes the transmission more robust in particular with WiFi links.

This plugin will be built only if the [CM256cc library](https://github.com/f4exb/cm256cc) IS installed in your system.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `RemoteOutput` even if no physical device is connected.

<h1>Channel plugins with special conditions</h1>

<h2>DSD (Digital Speech Decoder)</h2>

This is the `demoddsd` plugin. At present it can be used to decode the following digital speech formats:

  - DMR/MOTOTRBO
  - dPMR
  - D-Star
  - Yaesu System Fusion (YSF)
  - NXDN

It is based on the [DSDcc](https://github.com/f4exb/dsdcc) C++ library which is a rewrite of the original [DSD](https://github.com/szechyjs/dsd) program. So you will need to have DSDcc installed in your system. Please follow instructions in [DSDcc readme](https://github.com/f4exb/dsdcc/blob/master/Readme.md) to build and install DSDcc.

If you have one or more serial devices interfacing the AMBE3000 chip in packet mode you can use them to decode AMBE voice frames. For that purpose you will need to compile with [SerialDV](https://github.com/f4exb/serialDV) support. Please refer to this project Readme.md to compile and install SerialDV. Note that your user must be a member of group `dialout` (Ubuntu/Debian) or `uucp` (Arch) to be able to use the dongle.

Although such serial devices work with a serial interface at 400 kb in practice maybe for other reasons they are capable of handling only one conversation at a time. The software will allocate the device dynamically to a conversation with an inactivity timeout of 1 second so that conversations do not get interrupted constantly making the audio output too choppy. In practice you will have to have as many devices connected to your system as the number of conversations you would like to be handled in parallel.

Alternatively you can use [mbelib](https://github.com/szechyjs/mbelib) but mbelib comes with some copyright issues.

While DSDcc is intended to be patent-free, `mbelib` that it uses describes functions that may be covered by one or more U.S. patents owned by DVSI Inc. The source code itself should not be infringing as it merely describes possible methods of implementation. Compiling or using `mbelib` may infringe on patents rights in your jurisdiction and/or require licensing. It is unknown if DVSI will sell licenses for software that uses `mbelib`.

Possible copyright issues apart the audio quality with the DVSI AMBE chip is much better.

If you are not comfortable with this just do not install DSDcc and/or mbelib and the plugin will not be compiled and added to SDRangel. For packaged distributions just remove from the installation directory:

  - For Linux distributions: `plugins/channel/libdemoddsd.so`
  - For Windows distribution: `dsdcc.dll`, `mbelib.dll`, `plugins\channel\demoddsd.dll`

<h1>Software distributions</h1>

In the [releases](https://github.com/f4exb/sdrangel/releases) section one can find binary distributions for some Debian based distributions:

  - Ubuntu 18.10 (Cosmic)
  - Ubuntu 18.04 (Bionic)
  - Ubuntu 16.04 (Xenial)
  - Debian Stretch

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
  - `sudo apt-get -f install` this will install missing dependencies

The software is installed in `/opt/sdrangel` you can start it from the command line with:
  - `/opt/sdrangel/bin/sdrangel`

<h3>Ubuntu 18.04</h2>

The default CPU governor is now `powersave` which exhibits excessive CPU usage when running SDRangel. In the case of benchmarking and maybe high throughput usage it is recommended to switch to `performance` before running SDRangel by running the command: `sudo cpupower frequency-set --governor performance`. You can turn it back to `powersave` any time by running: `sudo cpupower frequency-set --governor powersave`. It is normal that with a lower CPU frequency the relative CPU usage rises for the same actual load. If not impairing operation this is normal and overall beneficial for heat and power consumption.

<h2>Windows distribution</h2>

This is the archive of the complete binary distribution that expands to the `sdrangel` directory. You can install it anywhere you like and click on `sdrangel.exe` to start.

<b>&#9888; Windows distribution is provided as a by product thanks to the Qt toolchain. The platform of choice to run SDRangel is definitely Linux and very little support can be given for this Windows distribution.</b>

<h1>Features</h1>

<h2>Changes from SDRangelove</h2>

See the v1.0.1 first official release [release notes](https://github.com/f4exb/sdrangel/releases/tag/v1.0.1)

<h2>To Do</h2>

Since version 3.3.2 the "todos" are in the form of tickets opened in the Issues section with the label "feature". When a specific release is targeted it will appear as a milestone. Thus anyone can open a "feature" issue to request a new feature.

Other ideas:

  - Enhance presets management (Edit, Move, Import/Export from/to human readable format like JSON).
  - Allow arbitrary sample rate for channelizers and demodulators (not multiple of 48 kHz). Prerequisite for polyphase channelizer
  - Implement polyphase channelizer
  - Level calibration
  - Even more demods. Contributors welcome!

<h1>Developer's notes</h1>

Please check the developer's specific [readme](./ReadmeDeveloper.md)
