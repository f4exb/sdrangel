SET libusbdir="C:\softs\libusb-1.0.21\MS64"
SET opencvdir="C:\softs\opencv\build\mw32\install\x86\mingw\bin"
SET libxml2dir="C:\softs\libxml2-2.7.8.win32"
SET libiconvdir="C:\softs\iconv-1.9.2.win32"
SET libzlib1dir="C:\softs\zlib-1.2.5"
SET pothosdir="C:\Program Files\PothosSDR"
SET pthreadsdir="C:\softs\pthreads-w32"
SET ffmpegdir="C:\softs\ffmpeg-20190308-9645147-win64-shared\bin"
SET qt5dir="C:\Qt\5.12.1\msvc2017_64\bin"
SET libopusdir="C:\softs\libopus_v1.3_msvc15\bin\x64"

echo copy internal libraries...
copy app\%1\sdrangel.exe %2
copy sdrbase\%1\sdrbase.dll %2
copy sdrbase\res.qrb %2\sdrbase.rcc
copy sdrgui\%1\sdrgui.dll %2
copy devices\%1\devices.dll %2
copy mbelib\%1\mbelib.dll %2
copy dsdcc\%1\dsdcc.dll %2
copy serialdv\%1\serialdv.dll %2
copy httpserver\%1\httpserver.dll %2
copy qrtplib\%1\qrtplib.dll %2
copy swagger\%1\swagger.dll %2
copy logging\%1\logging.dll %2
copy cm256cc\%1\cm256cc.dll %2
REM copy libhackrf\%1\libhackrf.dll %2
copy librtlsdr\%1\librtlsdr.dll %2
copy libairspy\%1\libairspy.dll %2
copy libairspyhf\%1\libairspyhf.dll %2

echo copy external dependencies...
copy %pothosdir%\bin\bladeRF.dll %2
copy %pothosdir%\bin\hackrf.dll %2
copy %pothosdir%\bin\libiio.dll %2
copy %pothosdir%\bin\LimeSuite.dll %2
copy %pothosdir%\bin\SoapySDR.dll %2

copy %libusbdir%\dll\libusb-1.0.dll %2
copy %pthreadsdir%\dll\x64\pthreadVC2.dll %2
REM copy %libxml2dir%\bin\libxml2.dll %2
REM copy %libiconvdir%\bin\iconv.dll %2
REM copy %libzlib1dir%\bin\zlib1.dll %2
REM copy %opencvdir%\opencv_ffmpeg2413.dll %2
REM copy %opencvdir%\libopencv_imgproc2413.dll %2
REM copy %opencvdir%\libopencv_highgui2413.dll %2
REM copy %opencvdir%\libopencv_core2413.dll %2
copy %ffmpegdir%\*.dll %2
copy %qt5dir%\Qt5Qml.dll %2
copy %libopusdir%\opus.dll %2

mkdir %2\plugins
mkdir %2\plugins\channelrx
mkdir %2\plugins\channeltx
mkdir %2\plugins\samplesource
mkdir %2\plugins\samplesink

echo copy channelrx...
copy plugins\channelrx\chanalyzer\%1\chanalyzer.dll %2\plugins\channelrx
copy plugins\channelrx\demodam\%1\demodam.dll %2\plugins\channelrx
copy plugins\channelrx\demodatv\%1\demodatv.dll %2\plugins\channelrx
copy plugins\channelrx\demodbfm\%1\demodbfm.dll %2\plugins\channelrx
copy plugins\channelrx\demoddatv\%1\demoddatv.dll %2\plugins\channelrx
copy plugins\channelrx\demoddsd\%1\demoddsd.dll %2\plugins\channelrx
REM copy plugins\channelrx\demodlora\%1\demodlora.dll %2\plugins\channelrx
copy plugins\channelrx\demodnfm\%1\demodnfm.dll %2\plugins\channelrx
copy plugins\channelrx\demodssb\%1\demodssb.dll %2\plugins\channelrx
copy plugins\channelrx\demodwfm\%1\demodwfm.dll %2\plugins\channelrx
copy plugins\channelrx\udpsink\%1\udpsink.dll %2\plugins\channelrx
copy plugins\channelrx\localsink\%1\localsink.dll %2\plugins\channelrx
copy plugins\channelrx\freqtracker\%1\freqtracker.dll %2\plugins\channelrx

echo copy channeltx...
copy plugins\channeltx\modam\%1\modam.dll %2\plugins\channeltx
REM copy plugins\channeltx\modatv\%1\modatv.dll %2\plugins\channeltx
copy plugins\channeltx\modnfm\%1\modnfm.dll %2\plugins\channeltx
copy plugins\channeltx\modssb\%1\modssb.dll %2\plugins\channeltx
copy plugins\channeltx\modwfm\%1\modwfm.dll %2\plugins\channeltx
copy plugins\channeltx\udpsource\%1\udpsource.dll %2\plugins\channeltx
copy plugins\channeltx\localsource\%1\localsource.dll %2\plugins\channeltx

echo copy samplesource...
copy plugins\samplesource\filesource\%1\inputfilesource.dll %2\plugins\samplesource
copy plugins\samplesource\testsource\%1\inputtestsource.dll %2\plugins\samplesource
copy plugins\samplesource\rtlsdr\%1\inputrtlsdr.dll %2\plugins\samplesource
copy plugins\samplesource\hackrfinput\%1\inputhackrf.dll %2\plugins\samplesource
copy plugins\samplesource\airspy\%1\inputairspy.dll %2\plugins\samplesource
copy plugins\samplesource\airspyhf\%1\inputairspyhf.dll %2\plugins\samplesource
copy plugins\samplesource\bladerf1input\%1\inputbladerf1.dll %2\plugins\samplesource
copy plugins\samplesource\bladerf2input\%1\inputbladerf2.dll %2\plugins\samplesource
copy plugins\samplesource\limesdrinput\%1\inputlimesdr.dll %2\plugins\samplesource
copy plugins\samplesource\plutosdrinput\%1\inputplutosdr.dll %2\plugins\samplesource
copy plugins\samplesource\remoteinput\%1\inputremote.dll %2\plugins\samplesource
copy plugins\samplesource\soapysdrinput\%1\inputsoapysdr.dll %2\plugins\samplesource
copy plugins\samplesource\localinput\%1\inputlocal.dll %2\plugins\samplesource

echo copy samplesink...
copy plugins\samplesink\filesink\%1\outputfilesink.dll %2\plugins\samplesink
copy plugins\samplesink\bladerf1output\%1\outputbladerf1.dll %2\plugins\samplesink
copy plugins\samplesink\bladerf2output\%1\outputbladerf2.dll %2\plugins\samplesink
copy plugins\samplesink\hackrfoutput\%1\outputhackrf.dll %2\plugins\samplesink
copy plugins\samplesink\limesdroutput\%1\outputlimesdr.dll %2\plugins\samplesink
copy plugins\samplesink\plutosdroutput\%1\outputplutosdr.dll %2\plugins\samplesink
copy plugins\samplesink\remoteoutput\%1\outputremote.dll %2\plugins\samplesink
copy plugins\samplesink\localoutput\%1\outputlocal.dll %2\plugins\samplesink
copy plugins\samplesink\soapysdroutput\%1\outputsoapysdr.dll %2\plugins\samplesink
