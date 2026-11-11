cmake_minimum_required(VERSION 3.16)
project(t3dash_sim C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
set(LV_LVGL_H_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/lvgl EXCLUDE_FROM_ALL)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_APP REQUIRED IMPORTED_TARGET sdl2)

set(VERSION_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/../include/version.h)
add_custom_command(
  OUTPUT ${VERSION_HEADER}
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/gen_version.py
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION
  COMMENT "Generating include/version.h"
  VERBATIM
)
add_custom_target(t3dash_version_header DEPENDS ${VERSION_HEADER})

add_executable(t3dash_sim
  main.cpp
  ../src/ui/lvgl/ui_lvgl.cpp
  ../src/ui/lvgl/ui_design_b.cpp
  ../src/ui/lvgl/ui_clock.cpp
  ../src/ui/lvgl/ui_brightness.cpp
  ../src/ui/lvgl/ui_bye.cpp
  ../assets/vw_gif.c
  ../assets/bye_gif.c
  ../assets/icons.c
  ../assets/font_speed_160.c
)

target_include_directories(t3dash_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../assets
)

target_link_libraries(t3dash_sim PRIVATE
  lvgl::lvgl
  PkgConfig::SDL2_APP
  m
)

add_dependencies(t3dash_sim t3dash_version_header)
