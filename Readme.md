![SDR Angel banner](/doc/img/sdrangel_banner.png)

**SDRangel** is an Open Source Qt5/OpenGL SDR and signal analyzer frontend to various hardware.

Although it keeps the same look and feel as its parent application **SDRangelove** it is a major redesign from it hitting more than half the lines of the code. Therefore the code base cannot be kept in sync anymore with its parent. It also contains enhancements and major differences. So it should now fly with its own wings and with its own name: **SDRangel**

<h1>Source code</h1>

<h2>Repository branches</h2>

- master: the production branch
- dev: the development branch
- fix: production fixes that can't wait
- legacy: the modified code from the parent application [hexameron rtl-sdrangelove](https://github.com/hexameron/rtl-sdrangelove) before a major redeisign of the code was carried out and sync was lost.

<h2>Untested plugins</h2>

These plugins come from the parent code base and have been maintained so that they compile but they are not being actively tested:

- Channels:
  - lora
  - tcpsrc (although it has evolved please use the udpsrc plugin instead)

<h1>Supported hardware</h1>

<h2>Multiple device support</h2>

From version 2 SDRangel can integrate more than one hardware device running concurrently.

<h2>Airspy</h2>

Airspy is supported through the libairspy library that should be installed in your system for proper build of the software and operation support. Add `libairspy-dev` to the list of dependencies to install.

If you use your own location for libairspy install directory you need to specify library and include locations. Example with `/opt/install/libairspy` with the following defines on `cmake` command line:

`-DLIBAIRSPY_LIBRARIES=/opt/install/libairspy/lib/libairspy.so -DLIBAIRSPY_INCLUDE_DIR=/opt/install/libairspy/include`

Please note that if you are using a recent version of libairspy (>= 1.0.6) the dynamic retrieval of sample rates is supported. To benefit from it you should modify the `plugins/samplesource/airspy/CMakeLists.txt` and change line `add_definitions(${QT_DEFINITIONS})` by `add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")`. In fact both lines are present with the last one commented out.

Be also aware that the lower rates (2.5 MS/s or 5 MS/s with modified firmware) are affected by a noise artifact so 10 MS/s is preferable for weak signal work or instrumentation. A decimation by 64 was implemented to facilitate narrow band work at 10 MS/s input rate.

<h2>BladeRF</h2>

BladeRF is supported through the libbladerf library that should be installed in your system for proper build of the software and operation support. Add `libbladerf-dev` to the list of dependencies to install.

If you use your own location for libbladeRF install directory you need to specify library and include locations. Example with `/opt/install/libbladerf` with the following defines on `cmake` command line:

`-DLIBBLADERF_LIBRARIES=/opt/install/libbladeRF/lib/libbladeRF.so -DLIBBLADERF_INCLUDE_DIR=/opt/install/libbladeRF/include`

<h2>FunCube Dongle</h2>

Both Pro and Pro+ are supported with the plugins in fcdpro and fcdproplus respectively. For the Pro+ the band filter selection is not effective as it is handled by the firmware using the center frequency.

The control interface is based on qthid and has been built in the software in the fcdhid library. You don't need anything else than libusb support. Library fcdlib is used to store the constants for each dongle type.

The Pro+ has trouble starting. The sound card interface is not recognized when you just plug it in and start SDRAngel. The workaround is to start qthid then a recording program like Audacity and start recording in Audacity. Then just quit Audacity without saving and quit qthid. After this operation the Pro+ should be recognized by SDRAngel until you unplug it.

<h2>HackRF</h2>

HackRF is supported through the libhackrf library that should be installed in your system for proper build of the software and operation support. Add `libhackrf-dev` to the list of dependencies to install. Please note that you will need a recent version (2015.07.2 at least) of libhackrf that supports the sequential listing of devices and the antenna power control (bias tee). To be safe anyway you may choose to build and install the Github version: `https://github.com/mossmann/hackrf.git`. Note also that the firmware must be updated to match the library version as per instructions found in the HackRF wiki.

If you use your own location for libhackrf install directory you need to specify library and include locations. Example with `/opt/install/libhackrf` with the following defines on `cmake` command line:

`-DLIBHACKRF_LIBRARIES=/opt/install/libhackrf/lib/libhackrf.so -DLIBHACKRF_INCLUDE_DIR=/opt/install/libhackrf/include`

HackRF is better used with a sampling rate of 4.8 MS/s and above. The 2.4 and 3.2 MS/s rates are considered experimental and are way out of specs of the ADC. You may or may not achieve acceptable results depending on the unit. A too low sampling rate will typically create ghost signals (images) and/or raise the noise floor.

<h2>RTL-SDR</h2>

RTL-SDR based dongles are supported through the librtlsdr library that should be installed in your system for proper build of the software and operation support. Add `librtlsdr-dev` to the list of dependencies to install.

If you use your own location for librtlsdr install directory you need to specify library and include locations. Example with `/opt/install/librtlsdr` with the following defines on `cmake` command line:

`-DLIBRTLSDR_LIBRARIES=/opt/install/librtlsdr/lib/librtlsdr.so -DLIBRTLSDR_INCLUDE_DIR=/opt/install/librtlsdr/include`

<h1>Plugins for special sample sources</h1>

<h2>File input</h2>

The file input plugin allows the playback of a recorded IQ file. Such a file is obtained using the recording feature. Press F7 to start recording and F8 to stop. The file has a fixed name `test.sdriq` created in the current directory.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is alwasys available in the list of devices as `FileSource[0]` even if no physical device is connected.

<h2>SDRdaemon input</h2>

This is the client side of the SDRdaemon server. See the [SDRdaemon](https://github.com/f4exb/sdrdaemon) project in this Github repository. You must specify the address and UDP port to which the server connects and samples will flow into the SDRangel application (default is `127.0.0.1`port `9090`). It uses the meta data to retrieve the sample flow characteristics such as sample rate and receiveng center frequency.

There is an automated skew rate compensation in place. During rate readjustemnt streaming can be suspended or signal glitches can occur for about one second.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is alwasys available in the list of devices as `SDRdaemon[0]` even if no physical device is connected.

<h2>SDRdaemonFEC input</h2>

This is a specialized client for the SDRdaemon server that matches the SDRdaemon with FEC option (`sdrdaemonfec` binary). The frame format is quite different from what it is without FEC and it also does not have the exact same functionnalities so a different plugin is more practiclal than trying to fit both versions in one plugin.

Using this scheme the remote operation is more robust in case conditions are not optimal. While SDRdaemon without FEC will work fine on copper or fiber lines the FEC version is recommended for WiFi links where even in good conditions some UDP packets might get lost.

This plugin will be built only if the [CM256cc library](https://github.com/f4exb/cm256cc) is installed in your system. You will then have to specify the include and library paths on the cmake command line. Say if you install cm256cc in `/opt/install/cm256cc` you will have to add `-DCM256CC_INCLUDE_DIR=/opt/install/cm256cc/include/cm256cc -DCM256CC_LIBRARIES=/opt/install/cm256cc/lib/libcm256cc.so` to the cmake commands.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is alwasys available in the list of devices as `SDRdaemonFEC[0]` even if no physical device is connected.

<h1>Channel plugins with special conditions</h1>

<h2>DSD (Digital Speech Decoder)</h2>

This is the `demoddsd` plugin. At present it can be used to decode the following digital speech formats:

  - DMR/MOTOTRBO
  - dPMR
  - D-Star

It is based on the [DSDcc](https://github.com/f4exb/dsdcc) C++ library which is a rewrite of the original [DSD](https://github.com/szechyjs/dsd) program. So you will need to have DSDcc installed in your system. Please follow instructions in [DSDcc readme](https://github.com/f4exb/dsdcc/blob/master/Readme.md) to build and install DSDcc. If you install it in a custom location say `/opt/install/dsdcc` you will need to add these defines to the cmake command: `-DLIBDSDCC_INCLUDE_DIR=/opt/install/dsdcc/include/dsdcc -DLIBDSDCC_LIBRARIES=/opt/install/dsdcc/lib/libdsdcc.so`

If you have one or more serial devices interfacing the AMBE3000 chip in packet mode you can use them to decode AMBE voice frames. For that purpose you will need to compile with [SerialDV](https://github.com/f4exb/serialDV) support. Please refer to this project Readme.md to compile and install SerialDV. If you install it in a custom location say `/opt/install/serialdv` you will need to add these defines to the cmake command: `-DLIBSERIALDV_INCLUDE_DIR=/opt/install/serialdv/include/serialdv -DLIBSERIALDV_LIBRARY=/opt/install/serialdv/lib/libserialdv.so`

Although such serial devices work with a serial interface at 400 kb in practice maybe for other reasons they are capable of handling only one conversation at a time. The software will allocate the device dynamically to a conversation with an inactivity timeout of 1 second so that conversations do not get interrupted constantly making the audio output too choppy. In practice you will have to have as many devices connected to your system as the number of conversations you would like to be handled in parallel. 

Note that this is not supported in Windows because of trouble with COM port support (contributors welcome!).

Alternatively you can use [mbelib](https://github.com/szechyjs/mbelib) but mbelib comes with some copyright issues (see next). If you have mbelib installed in a custom location, say `/opt/install/mbelib` you will need to add these defines to the cmake command: `-DLIBMBE_INCLUDE_DIR=/opt/install/mbelib/include -DLIBMBE_LIBRARY=/opt/install/mbelib/lib/libmbe.so`

Possible copyright issues apart (see next) the audio quality with the DVSI AMBE chip is much better.

While DSDcc is intended to be patent-free, `mbelib` that it uses describes functions that may be covered by one or more U.S. patents owned by DVSI Inc. The source code itself should not be infringing as it merely describes possible methods of implementation. Compiling or using `mbelib` may infringe on patents rights in your jurisdiction and/or require licensing. It is unknown if DVSI will sell licenses for software that uses `mbelib`.

Possible copyright issues apart the audio quality with the DVSI AMBE chip is much better.

If you are not comfortable with this just do not install DSDcc and/or mbelib and the plugin will not be compiled and added to SDRangel. For packaged distributions just remove from the installation directory:

  - For Linux distributions: `plugins/channel/libdemoddsd.so`
  - For Windows distributions: `dsdcc.dll`, `mbelib.dll`, `plugins\channel\demoddsd.dll`

<h1>Software build</h1>

<h2>Qt version</h2>

To be sure you will need at least Qt version 5.5. It definitely does not work with versions earlier than 5.3 but neither 5.3 nor 5.4 were tested.

  - Linux builds are made with 5.5.1
  - Windows 32 build is made with 5.5.1
  - Windows 64 build is made with 5.6 

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

  - `sudo apt-get install cmake g++ pkg-config libfftw3-dev libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev libboost-all-dev libasound2-dev pulseaudio liblz4-dev`
  - `mkdir build && cd build && cmake ../ && make`

`librtlsdr-dev` is in the `universe` repo. (utopic 14.10 amd64.)

<h2>Mint</h2>

Tested with Cinnamon 17.2. Since it is based on Ubuntu 14.04 LTS pleae follow instructions for this distribution (paragraph just above).

<h2>Debian</h2>

For any version of Debian you will need Qt5.

Debian 7 "wheezy" uses Qt4. Qt5 is available from the "wheezy-backports" repo, but this will remove Qt4. Debian 8 "jessie" uses Qt5.

For Debian Jessie or Stretch:

`sudo apt-get install cmake g++ pkg-config libfftw3-dev libusb-1.0-0-dev libusb-dev qt5-default qtbase5-dev qtchooser libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev librtlsdr-dev libboost-all-dev libasound2-dev pulseaudio`

`mkdir build && cd build && cmake ../ && make`

<h2>openSUSE</h2>

This has been tested with the bleeding edge "Thumbleweed" distribution:

`sudo zypper install cmake fftw3-devel gcc-c++ libusb-1_0-devel libqt5-qtbase-devel libQt5OpenGL-devel libqt5-qtmultimedia-devel libqt5-qttools-devel libQt5Network-devel libQt5Widgets-devel boost-devel alsa-devel pulseaudio liblz4 liblz4-devel`

Then you should be all set to build the software with `cmake` and `make` as discussed earlier.

  - Note1 for udev rules: installed udev rules for BladeRF and HackRF are targetted at Debian or Ubuntu systems that have a plugdev group for USB hotplug devices. This is not the case in openSUSE. To make the udev rules file compatible just remove the `GROUP` parameter on all lines and change `MODE` parameter to `666`.
  - Note2: A package has been created (thanks Martin!), see: [sdrangel](http://software.opensuse.org/download.html?project=home%3Amnhauke%3Asdr&package=sdrangel). It is based on the 1.0.1 release.

<h2>Fedora</h2>

This has been tested with Fedora 23 and 22:

  - `sudo dnf groupinstall "C Development Tools and Libraries"`
  - `sudo dnf install mesa-libGL-devel`
  - `sudo dnf install cmake gcc-c++ pkgconfig fftw-devel libusb-devel qt5-qtbase-devel qt5-qtmultimedia-devel qt5-qttools-devel boost-devel pulseaudio alsa-lib-devel liblz4 liblz4-devel`

Then you should be all set to build the software with `cmake` and `make` as discussed earlier.

  - Note for udev rules: the same as for openSUSE applies. This is detailed in the previous paragraph for openSUSE.

<h2>Manjaro</h2>

Tested with the 15.09 version with LXDE desktop (community supported). The exact desktop environment should not matter anyway. Since Manjaro is Arch Linux based prerequisites should be similar for Arch and all derivatives.

`sudo pacman -S cmake pkg-config fftw qt5-multimedia qt5-tools qt5-base libusb boost boost-libs pulseaudio lz4`

Then you should be all set to build the software with `cmake` and `make` as discussed earlier.

  - Note1 for udev rules: the same as for openSUSE and Fedora applies.
  - Note2: A package has been created in the AUR (thanks Mikos!), see: [sdrangel-git](https://aur.archlinux.org/packages/sdrangel-git). It is based on the `205fee6` commit of 8th December 2015.

<h2>Windows</h2>

<h3>Introduction, limitations, warnings...</h3>

This is new in version 1.1.3 and also experimental. Use at your own risk! This may or may not work on your machine and version of Windows. It was tested more or less successfully in native Windows 7, 8 and 10 however it does not work in a Virtualbox guest supposedly because it uses OpenGL ES 2.0 instead of the OpenGL desktop version (OpenGL 4.3) when it is running native and I think the OpenGL code in SDRangel is still not quite right to be compatible with the ES version (use of QtGLWidget instead of QtOpenGLWidget).

You should take note that the Windows scheduler is just a piece of crap and not suitable for near real time applications like SDRs. In any case you should make sure that the sdrangel.exe process does not take more than 35% of the global CPU (check this with Task Manager). Unload channel plugins if necessary. Promoting sdrangel.exe process to real time via Task Manager may or may not help but usually not. If you encounter any problem just grab a Linux installation CD or .iso file and get yourself a decent OS first. You have been warned!

There are no plugins for both flavours of Funcubes since it uses Alsa interface which is Linux exclusively. Changing for the Qt audio portable interface instead could be a solution that will be investigated in the future.

The SDRdaemon plug-in is present only in the 64 bit build version since version 1.1.4. The messaging system based on nanomsg works only in the 64 bit environment. However please be aware that the SDRdaemon plugin is not working well mainly due to the fact that it needs an OS with a decent scheduler and Windows is definitely not this sort of OS (see my previous warning). In fact depending on the case your mileage may vary however the Linux version works always beautifully so you know the options if you really want to use it!

<h3>Build environment</h3>

You will have to use QtCreator and its environment for that purpose. Build was done with the `Desktop_Qt_5_5_1_MinGW_32bit` tool-chain. Some other flavors might work. Please refer to Qt documentation for Qt Creator details.

You will need to add `CONFIG+=MINGW32` to the `qmake` options. In QtCreator open the `Projects` menu (the file icon on the left bar) and in the `Build steps` section open the `qmake` details collapsed section (click on the caret icon). Choose the build configuration for which you run the build (`debug` or `release`) and add `CONFIG+=MINGW32` to the `Additional arguments` line.

<h3>Dependencies</h3>

<h4>Boost</h4>

You only really need the Boost headers so there is no need to compile Boost itself. Just download an archive from the Boost website and unpack it somewhere. In our example it will be installed in `D:\boost_1_58_0`.

You then need to update the .pro files that depend on Boost. They are:

  - `sdrbase\sdrbase.pro`
  - `plugins\channel\chanalyzer\chanalyzer.pro`

Just update the following line with the location of your Boost installation:

  - `CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"`

<h4>USB support (libusb)</h4>

You have to download an archive of libusb that supports MinGW32 from the following [location](https://sourceforge.net/projects/libusb/files/libusb-1.0/). You will have the choice among various versions and various archive formats in each version folder. It works with version `1.0.19` and is untested with later version(s). In our example it will be installed in `D:\libusb-1.0.19`.

You then need to update the .pro files that depend on libusb. They are:

  - `libairspy\libairspy.pro`
  - `libhackrf\libhackrf.pro`
  - `librtlsdr\librtlsdr.pro`
  - `libbladerf\libbladerf.pro`

Just update the following lines with the location of your libusb installation:

  - `CONFIG(MINGW32):INCLUDEPATH += "D:\libusb-1.0.19\include\libusb-1.0"`
  - `CONFIG(MINGW32):LIBS += -LD:\libusb-1.0.19\MinGW32\dll -llibusb-1.0`

<h4>Airspy library (libairspy)</h4>

Download the source code or clone the git repository somewhere. It our example it will be installed in `D:\softs\libairspy`. Copy the header files (`*.h`) from `D:\softs\libairspy\libairspy\src` to the directory above (`D:\softs\libairspy\libairspy`).

You then need to update the .pro files that depend on libairspy. They are:

  - `libairspy\libairspy.pro`. Update the following line with the location of your libiarspy installation:
    - `CONFIG(MINGW32):LIBAIRSPYSRC = "D:\softs\libairspy\libairspy"`
  - `plugins\samplesource\airspy\airspy.pro`. Update the following line with the location of your libiarspy installation:
    - `CONFIG(MINGW32):LIBAIRSPYSRC = "D:\softs\libairspy"`

<h4>HackRF library (libhackrf)</h4>

Download the source code or clone the git repository somewhere. It our example it will be installed in `D:\softs\hackrf`. Copy the header files (`*.h`) from `D:\softs\hackrf\host\libhackrf\src` to the directory above (`D:\softs\hackrf\host\libhackrf`).

You then need to update the .pro files that depend on libhackrf. They are:

  - `libhackrf\libhackrf.pro`. Update the following line with the location of your libhackrf installation:
    - `CONFIG(MINGW32):LIBHACKRFSRC = "D:\softs\hackrf\host\libhackrf"`
  - `plugins\samplesource\hackrf\hackrf.pro`. Update the following line with the location of your libhackrf installation:
    - `CONFIG(MINGW32):LIBHACKRFSRC = "D:\softs\hackrf\host"`

<h4>RTL-SDR library (librtlsdr)</h4>

Download the source code or clone the git repository somewhere. It our example it will be installed in `D:\softs\librtlsdr`.

You then need to update the .pro files that depend on librtlsdr. They are:

  - `librtlsdr\librtlsdr.pro`. Update the following line with the location of your librtlsdr installation:
    - `CONFIG(MINGW32):LIBRTLSDRSRC = "D:\softs\librtlsdr"`
  - `plugins\samplesource\rtlsdr\rtlsdr.pro`. Update the following line with the location of your librtlsdr installation:
    - `CONFIG(MINGW32):LIBRTLSDRSRC = "D:\softs\librtlsdr"`

<h4>BladeRF library (libbladerf)</h4>

You need to download the 1.5.1 version specifically that is found [here](https://github.com/Nuand/bladeRF/archive/libbladeRF_v1.5.1.zip). Unzip it somewhere say in `D:\softs` So it will be installed in `D:\softs\bladeRF-libbladeRF_v1.5.1`. If your installation directory is different you need to update the dependent .pro files:

  - `libbladerf\libbladerf.pro`, update the following lines with the location of your bladeRF installation:
    - `CONFIG(MINGW32):LIBBLADERFSRC = "D:\softs\bladeRF-libbladeRF_v1.5.1"`
    - `CONFIG(MINGW32):LIBBLADERFCOMMONSRC = "D:\softs\bladeRF-libbladeRF_v1.5.1\host\common"`
    - `CONFIG(MINGW32):LIBBLADERFLIBSRC = "D:\softs\bladeRF-libbladeRF_v1.5.1\host\libraries\libbladeRF"`
  - `plugins\samplesource\bladerf\bladerf.pro`. Update the following line with the location of your BladeRF installation:
    - `CONFIG(MINGW32):LIBBLADERFSRC = "D:\softs\bladeRF\host\libraries\libbladeRF\include"`

<h3>Dependencies for DSD (Digital Speech Decoding) plugin</h3>

<h4>mbelib</h4>

You need to clone the [mbelib git repository](https://github.com/szechyjs/mbelib.git). Let's say you have cloned it to `D:\softs\mbelib`. If your cloned repository is different you will need to update the dependent .pro files:

  - `mbelib\mbelib.pro`
  - `dsdcc\dscc.pro`
  - `plugins\channel\demoddsd\demodsd.pro`

Changing the following lines:

  CONFIG(MINGW32):LIBMBELIBSRC = "D:\softs\mbelib"
  CONFIG(MINGW64):LIBMBELIBSRC = "D:\softs\mbelib"

<h4>DSDcc</h4>

You need to clone the [DSDcc git repository](https://github.com/f4exb/dsdcc.git). Let's say you have cloned it to `D:\softs\dsdcc`. If your cloned repository is different you will need to update the dependent .pro files:

  - `dsdcc\dscc.pro`
  - `plugins\channel\demoddsd\demodsd.pro`

Changing the following lines:

  CONFIG(MINGW32):LIBDSDCCSRC = "D:\softs\dsdcc"
  CONFIG(MINGW64):LIBDSDCCSRC = "D:\softs\dsdcc"

<h3>Build</h3>

Basically you open the project in QtCreator by selecting the `sdrangel.windows.pro` file in the source root directory and run the `build` command from the menu. This will eventually produce the `sdrangel.exe` executable and dependent library and plug-in DLLs in various parts of the build directory. See the Installation paragraph next for details on installing all files in a single place.

<h3>Installation</h3>

Then comes the tedious part of packaging everything in a single place so that you will just have to click on `sdrangel.exe` in the file explorer to start. Please follow the next steps for this purpose.

  - Make yourself an installation directory say `D:\Programs\sdrangel`
  - Assume the build directory is `D:\development\build-sdrangel.windows-Desktop_Qt_5_5_1_MinGW_32bit-Release` (assuming you compiled SDRangel for release)
  - Assume the source directory is `D:\development\sdrangel`
  - From the Qt group in the Windows start menu select the `Qt 5.5 for Desktop (Mingw...` console box
  - In this console type: `bin\windeployqt.exe --dir D:\Programs\sdrangel D:\development\build-sdrangel.windows-Desktop_Qt_5_5_1_MinGW_32bit-Release\app\release\sdrangel.exe D:\development\build-sdrangel.windows-Desktop_Qt_5_5_1_MinGW_32bit-Release\sdrbase\release\sdrbase.dll`
  - This copies all dependencies for Qt but alas nothing from our software so you will have to do this yourself. In the same console cd to the root of the build directory and type:
    - `D:\development\sdrangel\windows.install.bat release D:\Programs\sdrangel`
    - use `debug` in the place of `release` if you built the debug version

<h3>Running</h3>

You will need to install Zadig to get USB support for hardware devices. Please refer to [Zadig website](http://zadig.akeo.ie/) for details. Basically if you get things working for SDR# or HDSDR then it will work with SDRangel.

<h3>MinGW64 tool-chain</h3>

It is possible to use a MinGW64 tool-chain by following these steps:

  - Install MSys2 from [this page](http://msys2.github.io/). Follow all the steps.
  - Install Qt5 from MSys2 command line:
    - `pacman -Sy  mingw-w64-x86_64-qt5`
  - Install gcc/g++ from MSys2 command line:
    - `pacman -Sy mingw64/mingw-w64-x86_64-gcc`
  - Create a new "kit" in Qt Creator:
    - Go to "Projects" sub-menu from the left menu bar
    - Click on "Manage kits"
    - In "Compilers" tab add a compiler naming it "MinGW64" for example. In the compiler path specify the path to `g++` in your MSys2 installation (ex: `D:\msys64\mingw64\bin\g++.exe`)
    - In "Qt versions" tab add a Qt version and specify the path to the `qmake.exe` in your MSys2 installation (ex: `D:\msys64\mingw64\bin\qmake.exe`)
    - In "Kits" tab add a new kit and name it "MinGW64" for example.
      - In "Compiler" select the "MinGW64" compiler you created previously
      - In "Qt version" select the Qt version you created previously
    - You should now be able to use this "kit" for your build
    - In the "Build steps" section add `CONFIG+=MINGW64` in the "Additional arguments"

Use the `windeployqt.exe` of the MSys2 distribution to copy the base files to your target installation directory in a similar way as this is done for MinGW32 (see above).

The final packaging is done with the `windows64.install.bat` utility. Assuming `D:\development\sdrangel` is the root directory of your cloned source repository, `D:\msys64` is the installation directory of MSys2, `D:\libusb-1.0.19\MinGW64` is your libusb installation directory and `D:\Programs\sdrangel64` is your target installation directory do: `D:\development\sdrangel\windows64.install.bat release D:\Programs\sdrangel`. Modify the script if your MSys2 and libusb locations are different.

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
  - File input plugin in Linux: it is having trouble switching to another file source. The best option is to restart SDRangel. Strangely enough the Windows version does not seem to be affected maybe this is an ALSA only issue.
  - SDRdaemon FEC plugin: it has trouble doing the first connection or reconnecting to another device. The best option is to try then acknowledge the error message and restart SDRangel.

<h1>Limitations</h1>

  - Your hardware. Still SDRangel is relatively conservative on computer resources.

<h1>Features</h1>

<h2>Changes from SDRangelove</h2>

See the v1.0.1 first official relase [release notes](https://github.com/f4exb/sdrangel/releases/tag/v1.0.1)

<h2>To Do</h2>

  - Tx support for devices like Blade-RF or Hack-RF and simple file output (probably to start with)
  - Tx channels to feed Tx devices
  - Possibility to connect channels for example Rx to Tx or single Rx channel to dual Rx channel supporting MI(MO) features like 360 degree polarization detection.
  - Specialize plugins into channel and sample source plugins since both have almost complete different requirements and only little in common
  - 32 bit samples for the Channel Analyzer
  - Enhance presets management (Edit, Move, Import/Export from/to human readable format like JSON).
  - Headless mode based on a saved configuration in above human readable form
  - Allow arbitrary sample rate for channelizers and demodulators (not multiple of 48 kHz). Prerequisite for polyphase channelizer
  - Implement polyphase channelizer
  - Level calibration
  - Even more demods. Contributors welcome!

<h1>Developper's notes</h1>

<h2>Build options</h2>

The release type can be specified with the `-DBUILD_TYPE` cmake option. It takes the following values:

  - `RELEASE` (default): produces production release code i.e.optimized and no debug symbols
  - `RELEASEWITHDBGINFO`: optimized with debug info
  - `DEBUG`: unoptimized with debug info

You can specify whether or not you want to see debug messages printed out to the console with the `-DDEBUG_OUTPUT` cmake option:

  - `OFF` (default): no debug output
  - `ON`: debug output

You can add `-Wno-dev` on the `cmake` command line to avoid warnings.

<h2>Code organization</h2>

At the first subdirectory level `indclude` and `sdrbase` contain the common core components include and source files respectively. They are further broken down in subdirectories corresponding to a specific area:

  - `audio` contains the interface with the audio device(s)
  - `dsp` contains the common blocks for Digital Signal Processing like filters, scope and spectrum analyzer internals
  - `gui` contains the common Graphical User Interface components like the scope and spectrum analyzer controls and display
  - `plugin` contains the common blocks for managing plugins
  - `settings` contains components to manage presets and preferences
  - `util` contains common utilities such as the message queue

The `plugins` subdirectory contains the associated plugins used to manage devices and channel components. Naming convention of various items depend on the usage and Rx (reception side) or Tx (transmission side) affinity. Transmission side is yet to be created.

  - Receiver functions (Rx):
    - `samplesource`: Device managers:
      - `xxx` : Device manager (e.g. xxx = airspy)
        - `xxxinput.h/cpp` : Device interface
        - `xxxgui.h/cpp` : GUI
        - `xxxplugin.h/cpp` : Plugin interface
        - `xxxsettings.h/cpp` : Configuration manager
        - `xxxthread.h/cpp` : Reading samples
        - `xxx.pro` : Qt .pro file for Windows/Android build        
    - `channel`: Channel handlers:
      - `demodxxx` : Demodulator internal handler (e.g xxx = demodam)
        - `xxxdemod.h/cpp` : Demodulator core
        - `xxxdemodgui.h/cpp` : Demodulator GUI
        - `xxxplugin.h/cpp` : Plugin interface
        - `demodxxx.pro` : Qt .pro file for Windows/Android build
      - `xxxanalyzer` : Analyzer internal handler (e.g xxx = channel)
        - `xxxanalyzer.h/cpp` : Analyzer core
        - `xxxanalyzergui.h/cpp` : Analyzer GUI
        - `xxxanalyzerplugin.h/cpp` : Analyzer plugin manager
        - `xxxanalyzer.pro` : Qt .pro file for Windows/Android build
      - `xxxsrc` : Interface to the outside (e.g xxx = udp):
        - `xxxsrc.h/cpp` : Inteface core
        - `xxxsrcgui.h/cpp` : Interface GUI
        - `xxxsrcplugin/h/cpp` : Interface plugin manager
        - `xxxsrc.pro` : Qt .pro file for Windows/Android build
