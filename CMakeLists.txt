cmake_minimum_required(VERSION 3.10)
project(AudioC0re)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(NOT DEFINED ENV{PS2DEV})
    message(FATAL_ERROR "Set PS2DEV to your PS2SDK path!")
  endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -O2 -G0 -Wall \
    -Wno-unused-function -Wno-unused-variable -Wno-pointer-sign \
    -Wno-stringop-overflow -Wno-stringop-truncation")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")

find_package(SDL2 REQUIRED)

include_directories(
  ${CMAKE_SOURCE_DIR}/source
  $ENV{PS2SDK}/ports/include
  $ENV{PS2SDK}/ports/include/SDL2
  $ENV{PS2SDK}/ee/include
  $ENV{PS2SDK}/common/include
)

link_directories($ENV{PS2SDK}/ports/lib $ENV{PS2SDK}/ee/lib)

add_executable(${PROJECT_NAME}
  source/main.c
  source/input.c
  source/ui.c
  source/library.c
  source/metadata.c
  source/audio.c
  source/splash.c
  source/menu.c
  source/player.c
  source/log.c
  source/lang.c
  source/config.c
  source/settings.c
  source/external_impl.c
  source/logo_data.c
)

target_link_libraries(${PROJECT_NAME}
  ${SDL2_LIBRARIES}
  audsrv
  z
  kernel
  c
)
