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

<h1>Note on udev rules</h1>

On Linux you need specific files in `/etc/udev/rules.d` to be able to access the SDR hardware. Please refer to the respective hardware vendor instructions to install these files.

<h1>Software build on Linux</h1>

Plese consult the [Wiki page for compilation in Linux](https://github.com/f4exb/sdrangel/wiki/Compile-from-source-in-Linux).

<h1>Supported hardware</h1>

Details on how to compile support libraries and integrate them in the final build of SDRangel are detailed in the [Wiki page for compilation in Linux](https://github.com/f4exb/sdrangel/wiki/Compile-from-source-in-Linux) mentioned earlier.

<h1>Plugins for special devices</h1>

These plugins do not use any hardware device connected to your system.

<h2>File input</h2>

The [File input plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/fileinput) allows the playback of a recorded IQ file. Such a file is obtained using the recording feature. Click on the record button on the left of the main frequency dial to toggle recording. The file has a fixed name `test_<n>.sdriq` created in the current directory where `<n>` is the device tab index.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `FileInput[0]` even if no physical device is connected.

The `.sdriq` format produced are the 2x2 bytes I/Q samples with a header containing the center frequency of the baseband, the sample rate and the timestamp of the recording start. Note that this header length is a multiple of the sample size so the file can be read with a simple 2x2 bytes I/Q reader such as a GNU Radio file source block. It will just produce a short glitch at the beginning corresponding to the header data.

<h2>File output</h2>

The [File sink plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/filesink) allows the recording of the I/Q baseband signal produced by a transmission chain to a file in the `.sdriq` format thus readable by the file source plugin described just above.

Note that this plugin does not require any of the hardware support libraries nor the libusb library. It is always available in the list of devices as `FileSink[0]` even if no physical device is connected.

<h2>KiwiSDR</h2>

The [KiwiSDR plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/kiwisdr) is designed to enable connection to publicly available [KiwiSDR](http://kiwisdr.com/) receivers.

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

<h2>Local input</h2>

The [Local input plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesource/localinput) is similar to the Remote input discussed above but is the output of an internal pipe of samples instead of the local end of a remote sender over the network. It receives its samples flow from a [Local sink channel plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/channelrx/localsink) present in another source device set.

It can be used to use a narrow part of a receiver pass band to use it at a lower sample rate and thus "see" it in more detail.

<h2>Local output</h2>

The [Local output plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/samplesink/localoutput) is similar to the Remote output discussed above but is the input of an internal pipe of samples instead of sending samples over the network to a distant SDRangel remote source. It sends its samples flow to a [Local source channel plugin](https://github.com/f4exb/sdrangel/tree/dev/plugins/channeltx/localsource) present in another source device set.

It can be used to build a complex narrowband signal with multiple modulators and send it as part of a broader band transmission.

<h1>Channel plugins with special conditions</h1>

<h2>DSD (Digital Speech Decoder)</h2>

This is the `demoddsd` plugin. At present it can be used to decode the following digital speech formats:

  - DMR/MOTOTRBO
  - dPMR
  - D-Star
  - Yaesu System Fusion (YSF)
  - NXDN

It is based on the [DSDcc](https://github.com/f4exb/dsdcc) C++ library which is a rewrite of the original [DSD](https://github.com/szechyjs/dsd) program. So you will need to have DSDcc installed in your system. Please follow instructions in [DSDcc readme](https://github.com/f4exb/dsdcc/blob/master/Readme.md) to build and install DSDcc.

Note that you will need to compile with [SerialDV](https://github.com/f4exb/serialDV) support. This makes possible to use AMBE serial dongles or AMBE server connections. Please refer to this project Readme.md to compile and install SerialDV. Note that your user must be a member of group `dialout` (Ubuntu/Debian) or `uucp` (Arch) to be able to use the dongle.

Although such serial devices work with a serial interface at 400 kb in practice maybe for other reasons they are capable of handling only one conversation at a time. The software will allocate the device dynamically to a conversation with an inactivity timeout of 1 second so that conversations do not get interrupted constantly making the audio output too choppy. In practice you will have to have as many devices connected to your system as the number of conversations you would like to be handled in parallel.

When no AMBE devices or servers are activated [mbelib](https://github.com/szechyjs/mbelib) is used to decode AMBE frames but mbelib comes with some copyright issues.

While DSDcc is intended to be patent-free, `mbelib` that it uses describes functions that may be covered by one or more U.S. patents owned by DVSI Inc. The source code itself should not be infringing as it merely describes possible methods of implementation. Compiling or using `mbelib` may infringe on patents rights in your jurisdiction and/or require licensing. It is unknown if DVSI will sell licenses for software that uses `mbelib`.

Possible copyright issues apart the audio quality with the DVSI AMBE chip is much better.

<h1>Binary distributions</h1>

In the [releases](https://github.com/f4exb/sdrangel/releases) section of the Github repository one can find binary distributions for Ubuntu and Windows

<h2>Ubuntu</h2>

The `.deb` file is a Debian package that can be installed with `sudo apt-get install <.deb>` command. Occasionnally after this command you may have to force the installation of dependencies with the `sudo apt-get -f install` command.

The software is installed in `/usr/bin` for executables and `/usr/lib/sdrangel` for the dependent shared libraries. Therefore before executing `sdrangel` to start the program you have to set the `LD_LIBRARY_PATH` environment variable with: `export LD_LIBRARY_PATH=/usr/lib/sdrangel`. This is because not all libraries can have their `rpath` set.

<h2>Windows</h2>

The `.exe` file is a self extracting installation program. Just click on it and follow instructions on the successive panels.

The installation program creates a menu entry that can be invoked like any other Windows program.

Note that the archive can be opened, inspected and extracted using [7Zip](https://www.7-zip.org/).

<b>&#9888; Windows distribution is provided as a by product thanks to the Qt toolchain. The platform of choice to run SDRangel is definitely Linux and very little support can be given for this Windows distribution.</b>
