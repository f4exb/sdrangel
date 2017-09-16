<h2>Building and installing SDRangel for Windows</h2>

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

You have to download an archive of libusb that supports MinGW32 from the following [location](https://sourceforge.net/projects/libusb/files/libusb-1.0/). You will have the choice among various versions and various archive formats in each version folder. It works with version `1.0.20`. In our example it will be installed in `D:\softs\libusb-1.0.20`.

You then need to update the .pro files that depend on libusb. They are:

  - `libairspy\libairspy.pro`
  - `libhackrf\libhackrf.pro`
  - `librtlsdr\librtlsdr.pro`
  - `libbladerf\libbladerf.pro`

Just update the following lines with the location of your libusb installation:

  - `CONFIG(MINGW32):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"`
  - `CONFIG(MINGW32):LIBS += -LD:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0`

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

<h4>Libiio library (libiio) for PlutoSDR</h4>

This is needed for PlutoSDR support. It is found [here](https://github.com/analogdevicesinc/libiio) Clone it in `D:\softs` so that the source directory is `D:\softs\libiio`. The source path is declared in libiio.pro, devices.pro and plutosdrinput.pro:

  - `CONFIG(MINGW32):LIBIIOSRC = "D:\softs\libiio"`
  - `CONFIG(MINGW64):LIBIIOSRC = "D:\softs\libiio"`

Libiio has specific dependencies that should be installed. Copy commands are part of the installation bat files. You will find archives of the needed files [here](http://xmlsoft.org/sources/win32/). Relevant files are:

For win32:

iconv-1.9.2.win32.zip, libxml2-2.7.8.win32.zip, zlib-1.2.5.win32.zip. Unzip them in `D:\softs` so that the dlls can be found by the installation script at:

  - `D:\softs\iconv-1.9.2.win32\bin\iconv.dll`
  - `D:\softs\libxml2-2.7.8.win32\bin\libxml2.dll`
  - `D:\softs\zlib-1.2.5\bin\zlib1.dll`

For win64:

First cd to the `64bit` subdirectory. Then download `libxml2-2.9.3-win32-x86_64.7z` and unzip it in `D:\softs` so that the dlls can be found by the installation script at `D:\softs\libxml2-2.9.3-win32-x86_64\bin\libxml2-2.dll`

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
  - You cannot use the stock gcc/g++ because it is too new and not compatible with Qt 5.9. So you will have to fetch the 5.3.0 version from the archives:
    - `wget http://repo.msys2.org/mingw/x86_64/mingw-w64-x86_64-gcc-libs-5.3.0-5-any.pkg.tar.xz`
    - `wget http://repo.msys2.org/mingw/x86_64/mingw-w64-x86_64-gcc-5.3.0-5-any.pkg.tar.xz`
  - Install them in this order from the .xz package files:
    - `pacman -U mingw-w64-x86_64-gcc-libs-5.3.0-5-any.pkg.tar.xz`
    - `pacman -U mingw-w64-x86_64-gcc-5.3.0-5-any.pkg.tar.xz`
  - Create a new "kit" in Qt Creator:
    - Go to "Projects" sub-menu from the left menu bar
    - Click on "Manage kits"
    - In "Compilers" tab add a compiler naming it "MinGW64" for example. In the compiler path specify the path to `g++` in your MSys2 installation (ex: `D:\msys64\mingw64\bin\g++.exe`)
    - In "Qt versions" tab add a Qt version and specify the path to the `qmake.exe` in your MSys2 installation (ex: `D:\msys64\mingw64\bin\qmake.exe`)
    - In "Kits" tab add a new kit and name it "MinGW64" for example.
      - In "Compiler" select the "MinGW64" compiler you created previously
      - In "Qt version" select the Qt version you created previously
    - You should now be able to use this "kit" for your build
    - In the main "Build settings" panel:
		- In the "Build steps" section add `CONFIG+=MINGW64` in the "Additional arguments"
		- In the "Build steps" section in the "Make" subsection you will have to specify the same make as for the 32 bit build (ex: `D:\Qt\Tools\mingw530_32\bin\mingw32-make.exe`. The `make` provided by MSys2 does not work.
  - Create a command console shortcut:
    - Copy `D:\Qt\5.9.1\mingw53_32\bin\qtenv2.bat` to `D:\msys64\mingw64\bin`
    - Edit it to match the MSys2 installation with these two active lines:
      - `set PATH=D:\msys64\mingw64\bin;%PATH%`
      - `cd /D D:\msys64\mingw64`
	- Create a shortcut with target: `C:\Windows\System32\cmd.exe /A /Q /K D:\msys64\mingw64\bin\qtenv2.bat` 

Use the `windeployqt.exe` of the MSys2 distribution to copy the base files to your target installation directory in a similar way as this is done for MinGW32 (see above):

  - Open the command console with the shortcut you created
  - In this console type: `bin\windeployqt.exe --dir D:\Programs\sdrangel64 D:\development\build-sdrangel.windows-MinGW64-Release\app\release\sdrangel.exe D:\development\build-sdrangel.windows-MinGW64-Release\sdrbase\release\sdrbase.dll`

The final packaging is done with the `windows64.install.bat` utility. Assuming `D:\development\sdrangel` is the root directory of your cloned source repository, `D:\msys64` is the installation directory of MSys2, `D:\softs\libusb-1.0.20\MinGW64` is your libusb installation directory and `D:\Programs\sdrangel64` is your target installation directory do: `D:\development\sdrangel\windows64.install.bat release D:\Programs\sdrangel`. Modify the script if your MSys2 and libusb locations are different.
