# Fobos SDR input plugin

This plugin adds native SDRangel sample source support for RigExpert Fobos SDR devices.

It supports automatic backend selection:

- Agile firmware/API through `fobos_sdr.dll`
- regular/classic firmware/API through `fobos.dll`

Initial scope:

- Fobos SDR backend loaded at runtime: Agile `fobos_sdr.dll` first, regular `fobos.dll` fallback
- Device enumeration and open/close
- Center frequency control
- Sample rate selection
- Relative bandwidth control for Agile backend (shown but ignored by Regular backend)
- LNA gain control, 0..2
- VGA gain control, 0..31
- GPO control
- Internal/external clock selection
- Synchronous streaming into the SDRangel sample FIFO

Runtime notes:

- On Windows, place `fobos_sdr.dll` and its runtime dependencies next to the SDRangel executable or make them available through `PATH`.
- The plugin intentionally does not use developer-machine paths such as `C:\dev\...`.
- File logging is disabled by default. Configure with `-DFOBOS_DEBUG_FILE_LOG=ON` to write `sdrangel_fobos_source.log` for local diagnostics.

The plugin is intended as a native SDRangel sample source. It does not include analog video decoding or SDR# compatibility code.

Initial integration and hardware testing: Alex Antonov UT2UM Kyiv 2026.

## Windows runtime packaging

On Windows, official SDRangel builds are expected to provide the Fobos SDR runtime packages through the SDRangel Windows dependency repository:

- `external/windows/fobos-sdr` for Agile firmware/API
- `external/windows/fobos-regular` for regular/classic firmware/API

The plugin build copies `fobos_sdr.dll`, `fobos.dll`, and the required `libusb-1.0.dll` runtime to the SDRangel binary directory so users do not need to copy runtime DLLs manually.

The companion dependency repository updates are tracked separately as:

- `sdrangel-windows-libraries` PR #32 for Agile runtime
- `sdrangel-windows-libraries` PR #33 for regular/classic runtime
