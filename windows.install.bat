SET libusbdir="D:\softs\libusb-1.0.20\MinGW32"
SET opencvdir="D:\softs\opencv\build\mw32\install\x86\mingw\bin"

copy app\%1\sdrangel.exe %2
copy sdrbase\%1\sdrbase.dll %2
copy devices\%1\devices.dll %2
copy mbelib\%1\mbelib.dll %2
copy dsdcc\%1\dsdcc.dll %2
copy serialdv\%1\serialdv.dll %2
copy libhackrf\%1\libhackrf.dll %2
copy librtlsdr\%1\librtlsdr.dll %2
copy libairspy\%1\libairspy.dll %2
copy libbladerf\%1\libbladerf.dll %2
copy %libusbdir%\dll\libusb-1.0.dll %2
copy %opencvdir%\opencv_ffmpeg2413.dll %2
copy %opencvdir%\libopencv_imgproc2413.dll %2
copy %opencvdir%\libopencv_highgui2413.dll %2
copy %opencvdir%\libopencv_core2413.dll %2
mkdir %2\plugins
mkdir %2\plugins\channelrx
mkdir %2\plugins\channeltx
mkdir %2\plugins\samplesource
mkdir %2\plugins\samplesink
copy plugins\channelrx\chanalyzer\%1\chanalyzer.dll %2\plugins\channelrx
copy plugins\channelrx\chanalyzerng\%1\chanalyzerng.dll %2\plugins\channelrx
copy plugins\channelrx\demodam\%1\demodam.dll %2\plugins\channelrx
copy plugins\channelrx\demodatv\%1\demodatv.dll %2\plugins\channelrx
copy plugins\channelrx\demodbfm\%1\demodbfm.dll %2\plugins\channelrx
copy plugins\channelrx\demoddsd\%1\demoddsd.dll %2\plugins\channelrx
copy plugins\channelrx\demodlora\%1\demodlora.dll %2\plugins\channelrx
copy plugins\channelrx\demodnfm\%1\demodnfm.dll %2\plugins\channelrx
copy plugins\channelrx\demodssb\%1\demodssb.dll %2\plugins\channelrx
copy plugins\channelrx\demodwfm\%1\demodwfm.dll %2\plugins\channelrx
copy plugins\channelrx\tcpsrc\%1\tcpsrc.dll %2\plugins\channelrx
copy plugins\channelrx\udpsrc\%1\udpsrc.dll %2\plugins\channelrx
copy plugins\channeltx\modam\%1\modam.dll %2\plugins\channeltx
copy plugins\channeltx\modatv\%1\modatv.dll %2\plugins\channeltx
copy plugins\channeltx\modnfm\%1\modnfm.dll %2\plugins\channeltx
copy plugins\channeltx\modssb\%1\modssb.dll %2\plugins\channeltx
copy plugins\channeltx\modwfm\%1\modwfm.dll %2\plugins\channeltx
copy plugins\samplesource\filesource\%1\inputfilesource.dll %2\plugins\samplesource
copy plugins\samplesource\rtlsdr\%1\inputrtlsdr.dll %2\plugins\samplesource
copy plugins\samplesource\hackrfinput\%1\inputhackrf.dll %2\plugins\samplesource
copy plugins\samplesource\airspy\%1\inputairspy.dll %2\plugins\samplesource
copy plugins\samplesource\bladerfinput\%1\inputbladerf.dll %2\plugins\samplesource
copy plugins\samplesink\filesink\%1\outputfilesink.dll %2\plugins\samplesink
copy plugins\samplesink\bladerfoutput\%1\outputbladerf.dll %2\plugins\samplesink
copy plugins\samplesink\hackrfoutput\%1\outputhackrf.dll %2\plugins\samplesink
