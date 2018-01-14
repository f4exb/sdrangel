SET libusbdir="D:\softs\libusb-1.0.20\MinGW64"
SET msys2dir="D:\msys64"
SET opencvdir="D:\softs\opencv\build\mw64\install\x64\mingw\bin"
SET libxml2dir="D:\softs\libxml2-2.9.3-win32-x86_64"

copy %msys2dir%\mingw64\bin\libbz2-1.dll %2
copy %msys2dir%\mingw64\bin\libfreetype-6.dll %2
copy %msys2dir%\mingw64\bin\libglib-2.0-0.dll %2
copy %msys2dir%\mingw64\bin\libharfbuzz-0.dll %2
copy %msys2dir%\mingw64\bin\libiconv-2.dll %2
copy %msys2dir%\mingw64\bin\libintl-8.dll %2
copy %msys2dir%\mingw64\bin\libpcre-1.dll %2
copy %msys2dir%\mingw64\bin\libpcre16-0.dll %2
copy %msys2dir%\mingw64\bin\libpng16-16.dll %2
copy %msys2dir%\mingw64\bin\zlib1.dll %2
REM move %2\icudt56.dll %2\libicudt56.dll
copy %msys2dir%\mingw64\bin\libicuin58.dll %2
copy %msys2dir%\mingw64\bin\libicuind58.dll %2
copy %msys2dir%\mingw64\bin\libicuuc58.dll %2
copy %msys2dir%\mingw64\bin\libicudt58.dll %2
copy %msys2dir%\mingw64\bin\libgcc_s_seh-1.dll %2
copy "%msys2dir%\mingw64\bin\libstdc++-6.dll" %2
copy "%msys2dir%\mingw64\bin\libwinpthread-1.dll" %2
copy %msys2dir%\mingw64\bin\libgraphite2.dll %2
copy app\%1\sdrangel.exe %2
copy sdrbase\%1\sdrbase.dll %2
copy sdrgui\%1\sdrgui.dll %2
copy devices\%1\devices.dll %2
copy cm256cc\%1\cm256cc.dll %2
copy mbelib\%1\mbelib.dll %2
copy dsdcc\%1\dsdcc.dll %2
copy serialdv\%1\serialdv.dll %2
copy nanomsg\%1\nanomsg.dll %2
copy httpserver\%1\httpserver.dll %2
copy logging\%1\logging.dll %2
copy swagger\%1\swagger.dll %2
copy libhackrf\%1\libhackrf.dll %2
copy librtlsdr\%1\librtlsdr.dll %2
copy libairspy\%1\libairspy.dll %2
copy libairspyhf\%1\libairspyhf.dll %2
copy libbladerf\%1\libbladerf.dll %2
copy libsqlite3\%1\libsqlite3.dll %2
copy liblimesuite\%1\liblimesuite.dll %2
copy libiio\%1\libiio.dll %2
copy %libusbdir%\dll\libusb-1.0.dll %2
copy %libxml2dir%\bin\libxml2-2.dll %2
copy %opencvdir%\opencv_ffmpeg2413_64.dll %2
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
copy plugins\channeltx\udpsink\%1\udpsink.dll %2\plugins\channeltx
copy plugins\samplesource\filesource\%1\inputfilesource.dll %2\plugins\samplesource
copy plugins\samplesource\testsource\%1\inputtestsource.dll %2\plugins\samplesource
copy plugins\samplesource\sdrdaemonsource\%1\inputsdrdaemonsource.dll %2\plugins\samplesource
copy plugins\samplesource\rtlsdr\%1\inputrtlsdr.dll %2\plugins\samplesource
copy plugins\samplesource\hackrfinput\%1\inputhackrf.dll %2\plugins\samplesource
copy plugins\samplesource\airspy\%1\inputairspy.dll %2\plugins\samplesource
copy plugins\samplesource\airspyhf\%1\inputairspyhf.dll %2\plugins\samplesource
copy plugins\samplesource\bladerfinput\%1\inputbladerf.dll %2\plugins\samplesource
copy plugins\samplesource\limesdrinput\%1\inputlimesdr.dll %2\plugins\samplesource
copy plugins\samplesource\plutosdrinput\%1\inputplutosdr.dll %2\plugins\samplesource
copy plugins\samplesink\filesink\%1\outputfilesink.dll %2\plugins\samplesink
copy plugins\samplesink\bladerfoutput\%1\outputbladerf.dll %2\plugins\samplesink
copy plugins\samplesink\hackrfoutput\%1\outputhackrf.dll %2\plugins\samplesink
copy plugins\samplesink\limesdroutput\%1\outputlimesdr.dll %2\plugins\samplesink
copy plugins\samplesink\plutosdroutput\%1\outputplutosdr.dll %2\plugins\samplesink
