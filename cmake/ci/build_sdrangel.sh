#!/bin/bash -e

if [ "${TRAVIS_OS_NAME}" == "linux" ] || [ "${CI_LINUX}" == true ]; then
  debuild -i -us -uc -b
else
  mkdir -p build;  cd build
  cmake .. -GNinja ${CMAKE_CUSTOM_OPTIONS}

  case "${CMAKE_CUSTOM_OPTIONS}" in
    *BUNDLE=ON*)
      cmake --build . --target package
    ;;
    *)
      cmake --build .
    ;;
  esac
fi
