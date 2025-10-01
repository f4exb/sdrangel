# FileSinkTxt Plugin - Out-of-Tree SDRangel Plugin# FileSinkTxt Plugin - Out-Tree Build



## OverviewThis is an out-tree build of the FileSinkTxt plugin for SDRangel. This plugin extends the original FileSink to support saving IQ data in human-readable text format (.txt files).

A minimal SDRangel plugin for recording I/Q samples to text files and loading them back.

## Features

## Features

- **Record Button**: Start/stop recording I/Q samples to text file- Saves IQ data in text format with metadata headers

- **Filename Input**: Specify the output filename  - Supports .sdriq (binary), .wav, and .txt (text) formats

- **Choose File Button**: File dialog to select save location- Text format includes sample rate, center frequency, and timestamp information

- **Load File Button**: Load and display content from existing text files- Compatible with external analysis tools (Python, MATLAB, etc.)

- **Text Format**: Saves I/Q samples as "real,imaginary" pairs per line

## Building

## Building

### Prerequisites

### Prerequisites

- SDRangel source tree or installation- SDRangel installed and built (either from source or package)

- Qt5 or Qt6- Qt5 or Qt6 development libraries

- CMake 3.16+- CMake 3.16 or later

- C++ compiler

### Build Instructions

### Build Steps

1. **Clone/Download this plugin:**

   ```bash1. **Copy the plugin to a separate location** (important for out-tree build):

   git clone <this-repo>   ```bash

   cd filesinktxt   # Create a separate directory for out-tree plugins

   ```   mkdir -p ~/sdrangel-plugins

   cp -r /path/to/sdrangel/plugins/channelrx/filesinktxt ~/sdrangel-plugins/

2. **Create build directory:**   cd ~/sdrangel-plugins/filesinktxt

   ```bash   ```

   mkdir build && cd build

   ```2. Create a build directory:

   ```bash

3. **Configure and build:**   mkdir build

      cd build

   **If SDRangel is installed:**   ```

   ```bash

   cmake ..3. Configure with CMake:

   make   ```bash

   ```   # For in-source SDRangel builds (recommended):

      cmake -DSDRANGEL_SOURCE_DIR=/path/to/sdrangel/source ..

   **If using SDRangel source tree:**   

   ```bash   # Or for installed SDRangel:

   cmake .. -DSDRANGEL_DIR=/path/to/sdrangel-source   cmake ..

   make   ```

   ```   

   If SDRangel is installed in a non-standard location:

4. **Install:**   ```bash

   ```bash   cmake -DCMAKE_PREFIX_PATH=/path/to/sdrangel/install ..

   make install   ```

   ```

4. Build the plugin:

## Usage   ```bash

   make -j$(nproc)

1. Start SDRangel   ```

2. Add a new channel → Choose "File Sink Txt"

3. The minimal interface will show with:5. Install the plugin:

   - Filename input field   ```bash

   - "Choose File..." button for file selection   sudo make install

   - "Record" toggle button   ```

   - "Load File" button   

4. Use it to save/load I/Q data as text files   Or copy manually to SDRangel's plugin directory:

   ```bash

## File Format   cp libfilesinktxt.so ~/.local/lib/sdrangel/plugins/

   # or

The plugin saves I/Q samples in simple text format:   cp libfilesinktxt.so /usr/local/lib/sdrangel/plugins/

```   ```

# I/Q samples: real,imaginary

1234,-5678### Build Options

2345,-6789

...- **In-source build**: If building against SDRangel source, the CMakeLists.txt will automatically detect the relative paths

```- **Installed SDRangel**: For system-installed SDRangel, it searches common installation directories

- **Custom location**: Set `CMAKE_PREFIX_PATH` to point to custom SDRangel installation

## Plugin Structure

## Usage

- `filesinktxt.cpp/h` - Main plugin logic

- `filesinktxtgui.cpp/h` - Minimal GUIAfter installation, restart SDRangel. The FileSinkTxt plugin will appear in the channel plugins list.

- `filesinktxtplugin.cpp/h` - Plugin registration

- `filesinktxtgui.ui` - Qt UI layout- Select a filename with `.txt` extension

- `CMakeLists.txt` - Build system- Start recording to save IQ data in text format

- The output file will contain:

## License  ```

  # SDRangel Text File Record

GPL v3 - Same as SDRangel  # Sample Rate: 1000000 Hz
  # Center Frequency: 100000000 Hz
  # Timestamp: 2024-09-30T10:04:00
  # Format: I Q (real imaginary)
  
  -0.123 0.456
  -0.789 0.012
  ...
  ```

## File Format

The text files contain:
- Header with metadata (sample rate, frequency, timestamp)
- Space-separated I Q values (real imaginary)
- Normalized floating-point values (-1.0 to 1.0 range)

## Dependencies

This plugin requires:
- SDRangel core libraries (sdrbase, sdrgui, swagger)
- Qt Core and Widgets
- Standard C++ libraries

## Troubleshooting

If the build fails:
1. Ensure SDRangel is properly installed with development headers
2. Check that Qt development libraries are installed
3. Verify CMake can find SDRangel include files
4. Make sure plugin directory has write permissions during installation