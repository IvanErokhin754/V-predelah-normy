cmake_minimum_required(VERSION 3.16)
project(norma LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Тип сборки" FORCE)
endif()

option(NORMA_SYSTEM_SDL_HEADERS
       "Собирать с системными заголовками SDL2 вместо встроенных объявлений (src/engine/sdl_min.h)" OFF)

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Ядро правил: чистый C++17 без зависимостей ---
add_library(norma_core STATIC
  src/core/balance.cpp
  src/core/tasks.cpp
  src/core/game_state.cpp)

# --- Тесты ядра ---
add_executable(norma_tests tests/core_tests.cpp)
target_link_libraries(norma_tests PRIVATE norma_core)
enable_testing()
add_test(NAME core COMMAND norma_tests ${CMAKE_SOURCE_DIR}/data/balance.ini)

# --- Игра ---
add_executable(norma
  src/main.cpp
  src/engine/gl_min.cpp
  src/engine/font.cpp
  src/engine/texgen.cpp
  src/engine/renderer.cpp
  src/engine/audio.cpp
  src/game/level.cpp
  src/game/terminal.cpp
  src/game/game.cpp)
target_link_libraries(norma PRIVATE norma_core)

# SDL2: сначала пакет CMake (Homebrew, vcpkg, большинство дистрибутивов), иначе — просто библиотека.
find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
  target_link_libraries(norma PRIVATE SDL2::SDL2)
else()
  find_library(SDL2_LIBRARY
    NAMES SDL2 SDL2-2.0
    HINTS /opt/homebrew/lib /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu)
  if(NOT SDL2_LIBRARY)
    message(FATAL_ERROR
      "Не найдена библиотека SDL2.\n"
      "  macOS:  brew install sdl2\n"
      "  Ubuntu: sudo apt install libsdl2-dev\n"
      "Либо укажи путь вручную: -DSDL2_LIBRARY=/путь/к/libSDL2.dylib")
  endif()
  target_link_libraries(norma PRIVATE ${SDL2_LIBRARY})
endif()

if(NORMA_SYSTEM_SDL_HEADERS)
  target_compile_definitions(norma PRIVATE NORMA_SYSTEM_SDL_HEADERS)
  if(NOT TARGET SDL2::SDL2)
    find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2
      HINTS /opt/homebrew/include /usr/local/include /usr/include)
    target_include_directories(norma PRIVATE ${SDL2_INCLUDE_DIR})
  endif()
endif()

# Данные баланса кладём рядом с исполняемым файлом.
add_custom_command(TARGET norma POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:norma>/data)
