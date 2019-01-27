#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libbladerf

#DEFINES += BLADERF_OS_WINDOWS=1

CONFIG(MINGW32):LIBBLADERFSRC = "C:\softs\bladeRF"
CONFIG(MINGW32):LIBBLADERFCOMMONSRC = "C:\softs\bladeRF\host\common"
CONFIG(MINGW32):LIBBLADERFLIBSRC = "C:\softs\bladeRF\host\libraries\libbladeRF"
CONFIG(MINGW64):LIBBLADERFSRC = "C:\softs\bladeRF"
CONFIG(MINGW64):LIBBLADERFCOMMONSRC = "C:\softs\bladeRF\host\common"
CONFIG(MINGW64):LIBBLADERFLIBSRC = "C:\softs\bladeRF\host\libraries\libbladeRF"
INCLUDEPATH += $$PWD/mingw/include
INCLUDEPATH += $$PWD/mingw/common/include
INCLUDEPATH += $$PWD/mingw/libraries/libbladeRF/src
INCLUDEPATH += $$PWD/mingw/libraries/libbladeRF/src/backend
INCLUDEPATH += $$LIBBLADERFLIBSRC/include
INCLUDEPATH += $$LIBBLADERFLIBSRC/src
INCLUDEPATH += $$LIBBLADERFSRC/firmware_common
INCLUDEPATH += $$LIBBLADERFSRC/fpga_common/include
INCLUDEPATH += $$LIBBLADERFCOMMONSRC/include
INCLUDEPATH += $$LIBBLADERFCOMMONSRC/include/windows

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\libusb-1.0.19\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\libusb-1.0.19\include\libusb-1.0"
CONFIG(macx):INCLUDEPATH += "/opt/local/include"

SOURCES = $$LIBBLADERFCOMMONSRC/src/sha256.c\
    $$LIBBLADERFCOMMONSRC/src/dc_calibration.c\
    $$LIBBLADERFCOMMONSRC/src/parse.c\
    $$LIBBLADERFCOMMONSRC/src/devcfg.c\
    $$LIBBLADERFCOMMONSRC/src/conversions.c\
    $$LIBBLADERFCOMMONSRC/src/log.c\
    $$LIBBLADERFCOMMONSRC/src/str_queue.c\
#    $$LIBBLADERFCOMMONSRC/src/windows/clock_gettime.c\
#    $$LIBBLADERFCOMMONSRC/src/windows/getopt_long.c\
#    $$LIBBLADERFCOMMONSRC/src/windows/mkdtemp.c\
#    $$LIBBLADERFCOMMONSRC/src/windows/nanosleep.c\
#    $$LIBBLADERFCOMMONSRC/src/windows/setenv.c\
    $$LIBBLADERFSRC/host/misc/dev/lms_freqsel/freqsel.c\
    $$LIBBLADERFSRC/fpga_common/src/lms.c\
    $$LIBBLADERFSRC/fpga_common/src/band_select.c\
    $$LIBBLADERFLIBSRC/src/helpers/interleave.c\
    $$LIBBLADERFLIBSRC/src/helpers/timeout.c\
    $$LIBBLADERFLIBSRC/src/helpers/wallclock.c\
    $$LIBBLADERFLIBSRC/src/helpers/configfile.c\
    $$LIBBLADERFLIBSRC/src/helpers/file.c\
    $$LIBBLADERFLIBSRC/src/helpers/version.c\
    $$LIBBLADERFLIBSRC/src/driver/fpga_trigger.c\
    $$LIBBLADERFLIBSRC/src/driver/si5338.c\
    $$LIBBLADERFLIBSRC/src/driver/dac161s055.c\
    $$LIBBLADERFLIBSRC/src/driver/fx3_fw.c\
    $$LIBBLADERFLIBSRC/src/driver/smb_clock.c\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/dac_core.c\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/ad9361.c\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/util.c\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/platform.c\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/ad9361_api.c\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/adc_core.c\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/ad9361_conv.c\
    $$LIBBLADERFLIBSRC/src/driver/spi_flash.c\
    $$LIBBLADERFLIBSRC/src/driver/ina219.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf2/compatibility.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf2/capabilities.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf2/params.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf2/bladerf2.c\
    $$LIBBLADERFLIBSRC/src/board/board.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/flash.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/bladerf1.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/image.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/compatibility.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/calibration.c\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/capabilities.c\
    $$LIBBLADERFLIBSRC/src/expansion/xb100.c\
    $$LIBBLADERFLIBSRC/src/expansion/xb200.c\
    $$LIBBLADERFLIBSRC/src/expansion/xb300.c\
    $$LIBBLADERFLIBSRC/src/streaming/async.c\
    $$LIBBLADERFLIBSRC/src/streaming/sync_worker.c\
    $$LIBBLADERFLIBSRC/src/streaming/sync.c\
    $$LIBBLADERFLIBSRC/src/bladerf.c\
    $$LIBBLADERFLIBSRC/src/init_fini.c\
    $$LIBBLADERFLIBSRC/src/backend/dummy/dummy.c\
    $$LIBBLADERFLIBSRC/src/backend/backend.c\
    $$LIBBLADERFLIBSRC/src/backend/usb/usb.c\
    $$LIBBLADERFLIBSRC/src/backend/usb/libusb.c\
    $$LIBBLADERFLIBSRC/src/backend/usb/nios_access.c\
    $$LIBBLADERFLIBSRC/src/backend/usb/nios_legacy_access.c\
    $$LIBBLADERFLIBSRC/src/devinfo.c

HEADERS = $$PWD/mingw/common/include/host_config.h\
    $$PWD/mingw/libraries/libbladeRF/src/version.h\
    $$PWD/mingw/libraries/libbladeRF/src/backend/backend_config.h\
    $$LIBBLADERFCOMMONSRC/include/thread.h\
    $$LIBBLADERFCOMMONSRC/include/parse.h\
    $$LIBBLADERFCOMMONSRC/include/minmax.h\
    $$LIBBLADERFCOMMONSRC/include/rel_assert.h\
    $$LIBBLADERFCOMMONSRC/include/devcfg.h\
    $$LIBBLADERFCOMMONSRC/include/str_queue.h\
    $$LIBBLADERFCOMMONSRC/include/log.h\
    $$LIBBLADERFCOMMONSRC/include/dc_calibration.h\
    $$LIBBLADERFCOMMONSRC/include/sha256.h\
    $$LIBBLADERFCOMMONSRC/include/conversions.h\
    $$LIBBLADERFSRC/fpga_common/include/lms.h\
    $$LIBBLADERFSRC/fpga_common/include/band_select.h\
    $$LIBBLADERFLIBSRC/src/helpers/interleave.h\
    $$LIBBLADERFLIBSRC/src/helpers/wallclock.h\
    $$LIBBLADERFLIBSRC/src/helpers/timeout.h\
    $$LIBBLADERFLIBSRC/src/helpers/version.h\
    $$LIBBLADERFLIBSRC/src/helpers/configfile.h\
    $$LIBBLADERFLIBSRC/src/helpers/file.h\
    $$LIBBLADERFLIBSRC/src/driver/dac161s055.h\
    $$LIBBLADERFLIBSRC/src/driver/fpga_trigger.h\
    $$LIBBLADERFLIBSRC/src/driver/si5338.h\
    $$LIBBLADERFLIBSRC/src/driver/ina219.h\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/platform.h\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/util.h\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/dac_core.h\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/config.h\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/adc_core.h\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/common.h\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/ad9361.h\
    $$LIBBLADERFLIBSRC/src/driver/thirdparty/adi/ad9361_api.h\
    $$LIBBLADERFLIBSRC/src/driver/spi_flash.h\
    $$LIBBLADERFLIBSRC/src/driver/fx3_fw.h\
    $$LIBBLADERFLIBSRC/src/driver/smb_clock.h\
    $$LIBBLADERFLIBSRC/src/board/bladerf2/capabilities.h\
    $$LIBBLADERFLIBSRC/src/board/bladerf2/compatibility.h\
    $$LIBBLADERFLIBSRC/src/board/board.h\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/calibration.h\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/capabilities.h\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/compatibility.h\
    $$LIBBLADERFLIBSRC/src/board/bladerf1/flash.h\
    $$LIBBLADERFLIBSRC/src/expansion/xb300.h\
    $$LIBBLADERFLIBSRC/src/expansion/xb100.h\
    $$LIBBLADERFLIBSRC/src/expansion/xb200.h\
    $$LIBBLADERFLIBSRC/src/streaming/sync.h\
    $$LIBBLADERFLIBSRC/src/streaming/sync_worker.h\
    $$LIBBLADERFLIBSRC/src/streaming/metadata.h\
    $$LIBBLADERFLIBSRC/src/streaming/format.h\
    $$LIBBLADERFLIBSRC/src/streaming/async.h\
    $$LIBBLADERFLIBSRC/src/backend/backend.h\
    $$LIBBLADERFLIBSRC/src/backend/dummy/dummy.h\
    $$LIBBLADERFLIBSRC/src/backend/usb/nios_legacy_access.h\
    $$LIBBLADERFLIBSRC/src/backend/usb/nios_access.h\
    $$LIBBLADERFLIBSRC/src/backend/usb/usb.h\
    $$LIBBLADERFLIBSRC/src/devinfo.h\
    $$LIBBLADERFLIBSRC/include/bladeRF2.h\
    $$LIBBLADERFLIBSRC/include/libbladeRF.h\
    $$LIBBLADERFLIBSRC/include/bladeRF1.h

CONFIG(MINGW32):LIBS += -LC:\softs\libusb-1.0.21\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LC:\softs\libusb-1.0.21\MinGW64\dll -llibusb-1.0
macx {
    SOURCES =
    HEADERS =
    LIBS += -L/opt/local/lib -lbladerf
}

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
