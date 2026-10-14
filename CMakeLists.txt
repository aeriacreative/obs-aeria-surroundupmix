cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  src/plugin-main.cpp
  src/plugin-main.hpp
  src/plugin-macros.h
  src/audio/channel-layout.hpp
  src/audio/level-meter.hpp
  src/audio/level-meter.cpp
  src/audio/upmix-engine.hpp
  src/audio/upmix-engine.cpp
  src/audio/surround-filter.hpp
  src/audio/surround-filter.cpp
  src/audio/global-surround.hpp
  src/audio/global-surround.cpp
  src/audio/volmeter-hook.hpp
  src/audio/volmeter-hook.cpp
  src/frontend/circular-speaker-widget.hpp
  src/frontend/circular-speaker-widget.cpp
  src/frontend/channel-strip-widget.hpp
  src/frontend/channel-strip-widget.cpp
  src/frontend/control-dock.hpp
  src/frontend/control-dock.cpp
  src/frontend/monitor-dock.hpp
  src/frontend/monitor-dock.cpp
  src/frontend/dock-manager.hpp
  src/frontend/dock-manager.cpp
  src/state/filter-state.hpp
  src/state/filter-state.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(OS_WINDOWS)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
  )
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
