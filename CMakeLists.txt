cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_qt(COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/dispatch_queue.cpp
          src/dispatch_queue.h
          src/emulator.cpp
          src/emulator.h
          src/emuspy-source.cpp
          src/emuspy-source.h
          src/image.h
          src/input.h
          src/mips_analyzer.cpp
          src/mips_analyzer.h
          src/mips_converter.cpp
          src/mips_converter.h
          src/mips_decompiler.cpp
          src/mips_decompiler.h
          src/mips_instruction.cpp
          src/mips_instruction.h
          src/mips_interpreter.cpp
          src/mips_interpreter.h
          src/mips_memory.cpp
          src/mips_memory.h
          src/mips_types.h
          src/plugin-main.cpp
          src/skin.cpp
          src/skin.h
          src/tinyxml2.cpp
          src/tinyxml2.h
          src/winpp.h)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
