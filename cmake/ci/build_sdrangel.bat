mkdir build
cd build
cmake ..\ -G "%CMAKE_GENERATOR%" %CMAKE_CUSTOM_OPTIONS%
cmake --build . --config Release --target PACKAGE
