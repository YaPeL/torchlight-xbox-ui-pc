cmake_minimum_required(VERSION 3.16)
project(x360ui_installer C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  # One executable, no runtime DLL to ship.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W4 /utf-8)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra)
endif()

# tiny-AES-c (The Unlicense): AES-128 with CBC only.
add_library(tiny_aes STATIC third_party/tiny-aes/aes.c)
target_compile_definitions(tiny_aes PUBLIC CBC=1 ECB=0 CTR=0 AES128=1)
target_include_directories(tiny_aes PUBLIC third_party/tiny-aes)

# miniz (MIT): zip reading and inflate (the paks, the icons' PNG data).
add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)

# The pad prompts' icons (data/prompts, CC0) built into the executable.
file(GLOB_RECURSE PROMPT_PNGS "${CMAKE_CURRENT_SOURCE_DIR}/../data/prompts/*.png")
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/prompts_data.cpp
  COMMAND ${CMAKE_COMMAND} -DDIR=${CMAKE_CURRENT_SOURCE_DIR}/../data/prompts -DOUT=${CMAKE_CURRENT_BINARY_DIR}/prompts_data.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake
  DEPENDS ${PROMPT_PNGS} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake)

# proxy/cegui_exports.txt (the real DLL's check) and the release's proxies (optional), built in.
set(X360UI_PROXY_GOG "" CACHE FILEPATH "GOG's proxy CEGUIBase.dll to build into the installer")
set(X360UI_PROXY_STEAM "" CACHE FILEPATH "Steam's proxy CEGUIBase.dll to build into the installer")
set(EXPORTS_TXT ${CMAKE_CURRENT_SOURCE_DIR}/../proxy/cegui_exports.txt)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/exports_data.cpp
  COMMAND ${CMAKE_COMMAND} -DFILE=${EXPORTS_TXT} -DNAME=kCeguiExports -DTEXT=1 -DOUT=${CMAKE_CURRENT_BINARY_DIR}/exports_data.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake
  DEPENDS ${EXPORTS_TXT} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake)
# The release's tag (the release workflow passes it), for `notices`' source offer.
set(X360UI_VERSION "development" CACHE STRING "The release's tag, shown by x360ui-install notices")
# The licence and the third-party notices, built in: `x360ui-install notices` shows them (miniz's
# MIT notice must go with every copy of the executable, not only beside it in a release).
set(NOTICE_SOURCES)
foreach(pair "LICENSE:kLicenseText" "installer/third_party/NOTICES.md:kNoticesText" "installer/third_party/LGPL-2.1.txt:kLgplText")
  string(REPLACE ":" ";" parts "${pair}")
  list(GET parts 0 rel)
  list(GET parts 1 name)
  set(src ${CMAKE_CURRENT_SOURCE_DIR}/../${rel})
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp
    COMMAND ${CMAKE_COMMAND} -DFILE=${src} -DNAME=${name} -DTEXT=1 -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake
    DEPENDS ${src} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake)
  list(APPEND NOTICE_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp)
endforeach()
foreach(which GOG STEAM)
  if(which STREQUAL "GOG")
    set(name kProxyGog)
  else()
    set(name kProxySteam)
  endif()
  set(deps ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake)
  if(X360UI_PROXY_${which})
    get_filename_component(X360UI_PROXY_${which} "${X360UI_PROXY_${which}}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")
    if(NOT EXISTS ${X360UI_PROXY_${which}})
      message(FATAL_ERROR "X360UI_PROXY_${which}: ${X360UI_PROXY_${which}} does not exist")
    endif()
    list(APPEND deps ${X360UI_PROXY_${which}})
  endif()
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/proxy_${which}.cpp
    COMMAND ${CMAKE_COMMAND} -DFILE=${X360UI_PROXY_${which}} -DNAME=${name} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/proxy_${which}.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake
    DEPENDS ${deps})
endforeach()

add_library(x360ui_core STATIC src/stfs.cpp src/lzx.cpp src/xex.cpp src/pytext.cpp src/prompts.cpp src/convert.cpp
            src/game.cpp src/install.cpp
            ${CMAKE_CURRENT_BINARY_DIR}/prompts_data.cpp ${CMAKE_CURRENT_BINARY_DIR}/exports_data.cpp ${NOTICE_SOURCES}
            ${CMAKE_CURRENT_BINARY_DIR}/proxy_GOG.cpp ${CMAKE_CURRENT_BINARY_DIR}/proxy_STEAM.cpp)
if(WIN32)
  target_link_libraries(x360ui_core PUBLIC comdlg32 shell32 advapi32)
endif()
target_compile_definitions(x360ui_core PRIVATE X360UI_VERSION="${X360UI_VERSION}")
target_include_directories(x360ui_core PUBLIC src)
target_link_libraries(x360ui_core PUBLIC tiny_aes miniz)
# fit_icon computes in doubles as Python does: no fused multiply-add.
if(MSVC)
  target_compile_options(x360ui_core PRIVATE /fp:precise)
else()
  target_compile_options(x360ui_core PRIVATE -ffp-contract=off)
endif()

if(WIN32)
  # asInvoker: Windows never redirects a write it refuses (no VirtualStore); UAC is asked by the
  # installer itself, only when the game's folder needs it.
  add_executable(x360ui-install src/main.cpp src/x360ui-install.rc)
else()
  add_executable(x360ui-install src/main.cpp)
endif()
target_link_libraries(x360ui-install PRIVATE x360ui_core)
if(MSVC)
  # The manifest is the .rc's alone: the linker's own would be a second one. A window program
  # (no console of its own on a double click: the questions are windows), with main as its entry;
  # from a terminal it writes to that terminal's console (main.cpp).
  target_link_options(x360ui-install PRIVATE /MANIFEST:NO /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()
if(NOT MSVC AND NOT APPLE)
  # Linux: libstdc++ and libgcc inside the executable, so the AppImage needs only the system's libc.
  target_link_options(x360ui-install PRIVATE -static-libstdc++ -static-libgcc)
endif()

enable_testing()
add_executable(core_test tests/core_test.cpp)
target_link_libraries(core_test PRIVATE x360ui_core)
add_test(NAME core_test COMMAND core_test)
