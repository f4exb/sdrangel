#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libbladerf

DEFINES += BLADERF_OS_WINDOWS=1

CONFIG(MINGW32):LIBBLADERFSRC = "D:\softs\bladeRF"
CONFIG(MINGW32):LIBBLADERFCOMMONSRC = "D:\softs\bladeRF\host\common"
CONFIG(MINGW32):LIBBLADERFLIBSRC = "D:\softs\bladeRF\host\libraries\libbladeRF"
CONFIG(MINGW64):LIBBLADERFSRC = "D:\softs\bladeRF"
CONFIG(MINGW64):LIBBLADERFCOMMONSRC = "D:\softs\bladeRF\host\common"
CONFIG(MINGW64):LIBBLADERFLIBSRC = "D:\softs\bladeRF\host\libraries\libbladeRF"
INCLUDEPATH += $$LIBBLADERFLIBSRC/include
INCLUDEPATH += $$LIBBLADERFLIBSRC/src
INCLUDEPATH += $$LIBBLADERFSRC/firmware_common
INCLUDEPATH += $$LIBBLADERFSRC/fpga_common/include
INCLUDEPATH += $$LIBBLADERFCOMMONSRC/include
INCLUDEPATH += $$LIBBLADERFCOMMONSRC/include/windows
INCLUDEPATH += $$PWD/include

CONFIG(MINGW32):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"

SOURCES = $$LIBBLADERFLIBSRC/src/async.c\
    $$LIBBLADERFLIBSRC/src/bladerf_priv.c\
    $$LIBBLADERFLIBSRC/src/config.c\
    $$LIBBLADERFLIBSRC/src/device_identifier.c\
    $$PWD/src/file_ops.c\
    $$LIBBLADERFLIBSRC/src/flash_fields.c\
    $$LIBBLADERFLIBSRC/src/fx3_fw.c\
    $$LIBBLADERFLIBSRC/src/gain.c\
    $$LIBBLADERFLIBSRC/src/init_fini.c\
    $$LIBBLADERFLIBSRC/src/sync.c\
    $$LIBBLADERFLIBSRC/src/smb_clock.c\
    $$LIBBLADERFLIBSRC/src/tuning.c\
    $$LIBBLADERFLIBSRC/src/xb.c\
    $$LIBBLADERFLIBSRC/src/bladerf.c\
    $$LIBBLADERFLIBSRC/src/capabilities.c\
    $$LIBBLADERFLIBSRC/src/dc_cal_table.c\
    $$LIBBLADERFLIBSRC/src/devinfo.c\
    $$LIBBLADERFLIBSRC/src/flash.c\
    $$LIBBLADERFLIBSRC/src/fpga.c\
    $$LIBBLADERFLIBSRC/src/fx3_fw_log.c\
    $$LIBBLADERFLIBSRC/src/image.c\
    $$LIBBLADERFLIBSRC/src/si5338.c\
    $$LIBBLADERFLIBSRC/src/sync_worker.c\
    $$LIBBLADERFLIBSRC/src/trigger.c\
    $$LIBBLADERFLIBSRC/src/version_compat.c\
    $$LIBBLADERFLIBSRC/src/backend/backend.c\
    $$LIBBLADERFLIBSRC/src/backend/dummy.c\
    $$LIBBLADERFLIBSRC/src/backend/usb/libusb.c\
    $$LIBBLADERFLIBSRC/src/backend/usb/usb.c\
    $$LIBBLADERFLIBSRC/src/backend/usb/nios_access.c\
    $$LIBBLADERFLIBSRC/src/backend/usb/nios_legacy_access.c\
    $$LIBBLADERFSRC/fpga_common/src/band_select.c\
    $$LIBBLADERFSRC/fpga_common/src/lms.c\
    $$LIBBLADERFCOMMONSRC/src/conversions.c\
    $$LIBBLADERFCOMMONSRC/src/devcfg.c\
    $$LIBBLADERFCOMMONSRC/src/sha256.c

HEADERS = $$LIBBLADERFLIBSRC/src/async.h\
    $$LIBBLADERFLIBSRC/src/capabilities.h\
    $$LIBBLADERFLIBSRC/src/dc_cal_table.h\
    $$LIBBLADERFLIBSRC/src/devinfo.h\
    $$LIBBLADERFLIBSRC/src/flash.h\
    $$LIBBLADERFLIBSRC/src/fpga.h\
    $$LIBBLADERFLIBSRC/src/fx3_fw_log.h\
    $$LIBBLADERFLIBSRC/src/metadata.h\
    $$LIBBLADERFLIBSRC/src/sync.h\
    $$LIBBLADERFLIBSRC/src/smb_clock.h\
    $$LIBBLADERFLIBSRC/src/tuning.h\
    $$LIBBLADERFLIBSRC/src/xb.h\
    $$LIBBLADERFLIBSRC/src/bladerf_priv.h\
    $$LIBBLADERFLIBSRC/src/config.h\
    $$LIBBLADERFLIBSRC/src/device_identifier.h\
    $$LIBBLADERFLIBSRC/src/file_ops.h\
    $$LIBBLADERFLIBSRC/src/flash_fields.h\
    $$LIBBLADERFLIBSRC/src/fx3_fw.h\
    $$LIBBLADERFLIBSRC/src/gain.h\
    $$LIBBLADERFLIBSRC/src/si5338.h\
    $$LIBBLADERFLIBSRC/src/sync_worker.h\
    $$LIBBLADERFLIBSRC/src/trigger.h\
    $$LIBBLADERFLIBSRC/src/version_compat.h\
    $$LIBBLADERFLIBSRC/src/backend/backend.h\
    $$LIBBLADERFLIBSRC/src/backend/dummy.h\
    $$LIBBLADERFLIBSRC/src/backend/usb/usb.h\
    $$LIBBLADERFLIBSRC/src/backend/usb/nios_access.h\
    $$LIBBLADERFLIBSRC/src/backend/usb/nios_legacy_access.h\
    $$LIBBLADERFSRC/fpga_common/include/band_select.h\
    $$LIBBLADERFSRC/fpga_common/include/lms.h\
    $$LIBBLADERFCOMMONSRC/include/sha256.h\
    $$PWD/include/host_config.h\
    $$PWD/include/backend/backend_config.h\
    $$PWD/include/version.h

CONFIG(MINGW32):LIBS += -LD:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LD:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
