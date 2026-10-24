# The wizard's libraries, fetched at fixed versions and checked by hash (nothing of theirs is in the
# repository): SDL3 (zlib) and Dear ImGui (MIT) at the versions the ReXGlue SDK uses, built static;
# Noto Sans (SIL OFL 1.1), built into the executable. X360UI_DEPS_DIR: a folder holding the three
# archives (offline builds).
include(FetchContent)
set(X360UI_DEPS_DIR "" CACHE PATH "A folder with the wizard's archives, instead of downloading them")
function(x360ui_dep_url var file url)
  if(X360UI_DEPS_DIR AND EXISTS "${X360UI_DEPS_DIR}/${file}")
    set(${var} "${X360UI_DEPS_DIR}/${file}" PARENT_SCOPE)
  else()
    set(${var} "${url}" PARENT_SCOPE)
  endif()
endfunction()
x360ui_dep_url(sdl3_url SDL3-3.4.14.tar.gz
  https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz)
x360ui_dep_url(imgui_url v1.92.5.tar.gz https://github.com/ocornut/imgui/archive/refs/tags/v1.92.5.tar.gz)
x360ui_dep_url(noto_url NotoSans-v2.015.zip
  https://github.com/notofonts/latin-greek-cyrillic/releases/download/NotoSans-v2.015/NotoSans-v2.015.zip)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(sdl3 URL ${sdl3_url}
  URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb)
FetchContent_Declare(imgui URL ${imgui_url}
  URL_HASH SHA256=0eb50fe9aeba1a51f96b5843c7f630a32ed2e9362d693c61b87e4fa870cf826d)
FetchContent_Declare(noto URL ${noto_url}
  URL_HASH SHA256=0c34df072a3fa7efbb7cbf34950e1f971a4447cffe365d3a359e2d4089b958f5)

# SDL: static, with what a window, its renderer, the pads and the file pickers need.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_KMSDRM OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)          # Direct3D on Windows, OpenGL on Linux
set(SDL_RENDER_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sdl3 imgui noto)
# Their warnings are theirs (the project's -Wall -Wextra and /W4 are for its own code).
if(MSVC)
  target_compile_options(SDL3-static PRIVATE /W0)
else()
  target_compile_options(SDL3-static PRIVATE -w)
endif()

add_library(x360ui_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
target_include_directories(x360ui_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(x360ui_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(x360ui_imgui PUBLIC SDL3::SDL3-static)
if(MSVC)
  target_compile_options(x360ui_imgui PRIVATE /W0)
else()
  target_compile_options(x360ui_imgui PRIVATE -w)
endif()

# The font, built in (regular and bold).
set(X360UI_FONT_SOURCES)
foreach(pair "NotoSans-Regular.ttf:kFontRegular" "NotoSans-Bold.ttf:kFontBold")
  string(REPLACE ":" ";" parts "${pair}")
  list(GET parts 0 file)
  list(GET parts 1 name)
  set(src ${noto_SOURCE_DIR}/NotoSans/unhinted/ttf/${file})
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp
    COMMAND ${CMAKE_COMMAND} -DFILE=${src} -DNAME=${name} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake
    DEPENDS ${src} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake)
  list(APPEND X360UI_FONT_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp)
endforeach()
