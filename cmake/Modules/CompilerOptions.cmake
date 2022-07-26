include_guard(GLOBAL)

include(DetectArchitecture)

if (NOT APPLE)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  message(STATUS "Interprocedural optimization enabled")
else()
  message(STATUS "Interprocedural optimization is disabled for Mac OS")
endif()

if(WIN32)
  add_compile_definitions(
    NOMINMAX
    _USE_MATH_DEFINES
    _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
    _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT
  )
endif()

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wvla -Woverloaded-virtual -Wno-inconsistent-missing-override -ffast-math -fno-finite-math-only -ftree-vectorize)
endif()

if (SANITIZE_ADDRESS)
    message(STATUS "Activate address sanitization")
    if(MSVC)
      set(ASAN_LIB_ARCH ${MSVC_CXX_ARCHITECTURE_ID})
      string(TOLOWER ${ASAN_LIB_ARCH} ASAN_LIB_ARCH)
      if(ASAN_LIB_ARCH STREQUAL "x86")
        set(ASAN_LIB_ARCH "i386")
      elseif(ASAN_LIB_ARCH STREQUAL "x64")
        set(ASAN_LIB_ARCH "x86_64")
      endif()
      add_compile_options(/fsanitize=address)
      link_libraries(clang_rt.asan_dynamic-${ASAN_LIB_ARCH} clang_rt.asan_dynamic_runtime_thunk-${ASAN_LIB_ARCH})
      add_link_options(/wholearchive:clang_rt.asan_dynamic_runtime_thunk-${ASAN_LIB_ARCH}.lib)
    else()
      add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
      add_link_options(-fsanitize=address)
    endif()
endif()
