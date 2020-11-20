include_guard(GLOBAL)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(C_CLANG 1)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(C_GCC 1)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(C_MSVC 1)
endif()
