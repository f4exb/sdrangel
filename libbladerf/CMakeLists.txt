project(bladerf)

find_package(LibUSB)

add_definitions(-DBLADERF_OS_LINUX)

set(bladerf_SOURCES
    ${LIBBLADERFLIBSRC}/src/async.c 
    ${LIBBLADERFLIBSRC}/src/bladerf_priv.c 
    ${LIBBLADERFLIBSRC}/src/config.c
    ${LIBBLADERFLIBSRC}/src/device_identifier.c
    src/file_ops.c
    ${LIBBLADERFLIBSRC}/src/flash_fields.c 
    ${LIBBLADERFLIBSRC}/src/fx3_fw.c
    ${LIBBLADERFLIBSRC}/src/gain.c
    ${LIBBLADERFLIBSRC}/src/init_fini.c
    ${LIBBLADERFLIBSRC}/src/sync.c
    ${LIBBLADERFLIBSRC}/src/smb_clock.c
    ${LIBBLADERFLIBSRC}/src/tuning.c
    ${LIBBLADERFLIBSRC}/src/xb.c
    ${LIBBLADERFLIBSRC}/src/bladerf.c
    ${LIBBLADERFLIBSRC}/src/capabilities.c
    ${LIBBLADERFLIBSRC}/src/dc_cal_table.c
    ${LIBBLADERFLIBSRC}/src/devinfo.c
    ${LIBBLADERFLIBSRC}/src/flash.c
    ${LIBBLADERFLIBSRC}/src/fpga.c
    ${LIBBLADERFLIBSRC}/src/fx3_fw_log.c
    ${LIBBLADERFLIBSRC}/src/image.c
    ${LIBBLADERFLIBSRC}/src/si5338.c
    ${LIBBLADERFLIBSRC}/src/sync_worker.c
    ${LIBBLADERFLIBSRC}/src/trigger.c
    ${LIBBLADERFLIBSRC}/src/version_compat.c
    ${LIBBLADERFLIBSRC}/src/backend/backend.c
    ${LIBBLADERFLIBSRC}/src/backend/dummy.c
    ${LIBBLADERFLIBSRC}/src/backend/usb/libusb.c
    ${LIBBLADERFLIBSRC}/src/backend/usb/usb.c
    ${LIBBLADERFLIBSRC}/src/backend/usb/nios_access.c
    ${LIBBLADERFLIBSRC}/src/backend/usb/nios_legacy_access.c
    ${LIBBLADERFSRC}/fpga_common/src/band_select.c
    ${LIBBLADERFSRC}/fpga_common/src/lms.c
    ${LIBBLADERFCOMMONSRC}/src/conversions.c
    ${LIBBLADERFCOMMONSRC}/src/devcfg.c
    ${LIBBLADERFCOMMONSRC}/src/sha256.c
)

set(bladerf_HEADERS
    ${LIBBLADERFLIBSRC}/src/async.h
    ${LIBBLADERFLIBSRC}/src/capabilities.h
    ${LIBBLADERFLIBSRC}/src/dc_cal_table.h
    ${LIBBLADERFLIBSRC}/src/devinfo.h
    ${LIBBLADERFLIBSRC}/src/flash.h
    ${LIBBLADERFLIBSRC}/src/fpga.h
    ${LIBBLADERFLIBSRC}/src/fx3_fw_log.h
    ${LIBBLADERFLIBSRC}/src/metadata.h
    ${LIBBLADERFLIBSRC}/src/sync.h
    ${LIBBLADERFLIBSRC}/src/smb_clock.h
    ${LIBBLADERFLIBSRC}/src/tuning.h
    ${LIBBLADERFLIBSRC}/src/xb.h
    ${LIBBLADERFLIBSRC}/src/bladerf_priv.h
    ${LIBBLADERFLIBSRC}/src/config.h
    ${LIBBLADERFLIBSRC}/src/device_identifier.h
    ${LIBBLADERFLIBSRC}/src/file_ops.h
    ${LIBBLADERFLIBSRC}/src/flash_fields.h
    ${LIBBLADERFLIBSRC}/src/fx3_fw.h
    ${LIBBLADERFLIBSRC}/src/gain.h
    ${LIBBLADERFLIBSRC}/src/si5338.h
    ${LIBBLADERFLIBSRC}/src/sync_worker.h
    ${LIBBLADERFLIBSRC}/src/trigger.h
    ${LIBBLADERFLIBSRC}/src/version_compat.h
    ${LIBBLADERFLIBSRC}/src/backend/backend.h
    ${LIBBLADERFLIBSRC}/src/backend/dummy.h
    ${LIBBLADERFLIBSRC}/src/backend/usb/usb.h
    ${LIBBLADERFLIBSRC}/src/backend/usb/nios_access.h
    ${LIBBLADERFLIBSRC}/src/backend/usb/nios_legacy_access.h
    ${LIBBLADERFSRC}/fpga_common/include/band_select.h
    ${LIBBLADERFSRC}/fpga_common/include/lms.h
    ${LIBBLADERFCOMMONSRC}/include/sha256.h
    include/host_config.h
    include/backend/backend_config.h
    include/version.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}
    ${LIBBLADERFLIBSRC}/include
    ${LIBBLADERFLIBSRC}/src
    ${LIBBLADERFSRC}/firmware_common
    ${LIBBLADERFSRC}/fpga_common/include
    ${LIBBLADERFCOMMONSRC}/include
    ${LIBBLADERFCOMMONSRC}/include/windows
    ./include
)

add_definitions(-DQT_SHARED)

add_library(bladerf SHARED
    ${bladerf_SOURCES}
)

target_link_libraries(bladerf
    ${LIBUSB_LIBRARIES}
)

install(TARGETS bladerf DESTINATION lib)
