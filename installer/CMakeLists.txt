cmake_minimum_required(VERSION 3.16)
project(x360ui_installer C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  # One executable, no runtime DLL to ship.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra)
endif()

# tiny-AES-c (The Unlicense): AES-128 with CBC only.
add_library(tiny_aes STATIC third_party/tiny-aes/aes.c)
target_compile_definitions(tiny_aes PUBLIC CBC=1 ECB=0 CTR=0 AES128=1)
target_include_directories(tiny_aes PUBLIC third_party/tiny-aes)

add_library(x360ui_core STATIC src/stfs.cpp src/lzx.cpp src/xex.cpp)
target_include_directories(x360ui_core PUBLIC src)
target_link_libraries(x360ui_core PUBLIC tiny_aes)

add_executable(x360ui-install src/main.cpp)
target_link_libraries(x360ui-install PRIVATE x360ui_core)
if(NOT MSVC AND NOT APPLE)
  # Linux: libstdc++ and libgcc inside the executable, so the AppImage needs only the system's libc.
  target_link_options(x360ui-install PRIVATE -static-libstdc++ -static-libgcc)
endif()

enable_testing()
add_executable(core_test tests/core_test.cpp)
target_link_libraries(core_test PRIVATE x360ui_core)
add_test(NAME core_test COMMAND core_test)
