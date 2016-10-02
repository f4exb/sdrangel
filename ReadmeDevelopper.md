<h1>SDRangel Developper's notes</h1>

<h2>Build options</h2>

The release type can be specified with the `-DBUILD_TYPE` cmake option. It takes the following values:

  - `RELEASE` (default): produces production release code i.e.optimized and no debug symbols
  - `RELEASEWITHDBGINFO`: optimized with debug info
  - `DEBUG`: unoptimized with debug info

You can specify whether or not you want to see debug messages printed out to the console with the `-DDEBUG_OUTPUT` cmake option:

  - `OFF` (default): no debug output
  - `ON`: debug output

You can add `-Wno-dev` on the `cmake` command line to avoid warnings.

<h2>Global code structure and flow</h2>

![SDRangel code map](./doc/img/SDRangelFlow.png)

The existing receiving flow is represented with green boxes. The futrue Tx flow with red boxes. Some classes and file paths in the Rx part were renamed to avoid collision with future Tx names in this case the old name appears below the present name in italics.

<h3>Rx path</h3>

 - The I/Q samples are collected from a physical device or UDP flow with a device plugin that derives from the `DeviceSampleSource` class.
 - These I/Q samples are downsampled by a factor set in the device plugin GUI and fed into a `BasebandSampleSink`
 - The `DownChannelizer`class downsamples further down (or not) the baseband I/Q samples depending on the requirements of the Rx plugin. It cascades the downsampling from the center, left or right half of the baseband in order to fit the Rx plugin bandwidth. It contains the NCO to adjust to the Rx plugin center frequency.
 - The special `FileSource` plugin reads a file that was recorded using the `FileRecord` class directly into the baseband as there is no downsampling from the sample rate at which the file was recorded.
 - The baseband I/Q samples can be recorded to a file using the `FileRecord` class
 
<h3>Device sample source plugins</h3>

At present the following plugins are available:

  - `AirspyXxx` classes in `plugins/samplesource/airspy`: Inteface with Airspy devices
  - `BladeRFXxx` classes in `plugins/samplesource/bladerf`: Inteface with BladeRF devices
  - `BladeRFXxx` classes in `plugins/samplesource/bladerf`: Inteface with BladeRF devices
  - `FCDProXxx` classes in `plugins/samplesource/fcdpro`: Inteface with Funcube Pro devices
  - `FCDProPlusXxx` classes in `plugins/samplesource/fcdproplus`: Inteface with Funcube Pro+ devices
  - `HackRFXxx` classes in `plugins/samplesource/hackrf`: Inteface with HackRF devices
  - `RTLSDRXxx` classes in `plugins/samplesource/rtlsdr`: Inteface with RTL-SDR devices
  - `SDRDaemonXxx` classes in `plugins/samplesource/sdrdaemon`: Special inteface collecting I/Q samples from an UDP flow sent by a remote device using [SDRdaemon](https://github.com/f4exb/sdrdaemon).
  - `SDRDaemonFECXxx` classes in `plugins/samplesource/sdrdaemonfec`: Special inteface collecting I/Q samples from an UDP flow sent by a remote device using [SDRdaemon](https://github.com/f4exb/sdrdaemon) with FEC protection of blocks.
  - `FileSource` classes in `plugins/samplesource/filesource`: Special inteface reading I/Q samples from a file directly into the baseband skipping the downsampling block

<h3>Channel receiver (Rx) plugins</h3>

At present the following plugins are available:

  - `ChannelAnalyzerXxx` classes in `plugins/channelrx/chanalyzer`: Signal analysis tool pretty much like a DSA/DSO signal analyzer like the venerable HP 4406A (although still far from it!)
  - `AMDemodXxx` classes in `plugins/channelrx/demodam`: AM demodulator with audio output
  - `BFMDemodXxx` classes in `plugins/channelrx/demodbfm`: Broadcast FM demodulator with audio mono/stereo output and RDS
  - `DSDDemodXxx` classes in `plugins/channelrx/demoddsd`: Digital Speech demodulator/decoder built on top of the [DSDcc library](https://github.com/f4exb/dsdcc). Produces audio output and some communication data from various digital voice standards: DMR, dPMR, D-Star, Yaesu System Fusion (YSF).
  - `LoraDemodXxx` classes in `plugins/channelrx/demodlora`: Decodes [LoRa](http://www.semtech.com/images/datasheet/an1200.22.pdf) transmissions. This is legacy code that is not very well maintained so it may or may not work.
  - `NFMDemodXxx` classes in `plugins/channelrx/demodnfm`: Narrowband FM demodulator with audio output.
  - `SSBDemodXxx` classes in `plugins/channelrx/demodssb`: SSB/DSB/CW demodulator with audio output.
  - `WFMDemodXxx` classes in `plugins/channelrx/demodwfm`: Wideband FM demodulator with audio output. This is a basic demodulator.
  - `TCPSrcXxx` classes in `plugins/channelrx/tcpsrc`: Sends channel I/Q samples via TCP
  - `UDPSrcXxx` classes in `plugins/channelrx/udpsrc`: Sends channel I/Q or FM demodulated samples via UDP

<h2>Source tree structure</h2>

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
