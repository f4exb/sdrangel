SET libusbdir="D:\libusb-1.0.20\MinGW32"

copy app\%1\sdrangel.exe %2
copy sdrbase\%1\sdrbase.dll %2
copy lz4\%1\lz4.dll %2
copy %libusbdir%\dll\libusb-1.0.dll %2
mkdir %2\plugins
mkdir %2\plugins\channel
mkdir %2\plugins\samplesource
copy plugins\channel\chanalyzer\%1\chanalyzer.dll %2\plugins\channel
copy plugins\channel\demodam\%1\demodam.dll %2\plugins\channel
copy plugins\channel\demodbfm\%1\demodbfm.dll %2\plugins\channel
copy plugins\channel\demodlora\%1\demodlora.dll %2\plugins\channel
copy plugins\channel\demodnfm\%1\demodnfm.dll %2\plugins\channel
copy plugins\channel\demodssb\%1\demodssb.dll %2\plugins\channel
copy plugins\channel\demodwfm\%1\demodwfm.dll %2\plugins\channel
copy plugins\channel\tcpsrc\%1\tcpsrc.dll %2\plugins\channel
copy plugins\channel\udpsrc\%1\udpsrc.dll %2\plugins\channel
copy plugins\samplesource\filesource\%1\inputfilesource.dll %2\plugins\samplesource
copy plugins\samplesource\sdrdaemon\%1\inputsdrdaemon.dll %2\plugins\samplesource
copy plugins\samplesource\rtlsdr\%1\inputrtlsdr.dll %2\plugins\samplesource
