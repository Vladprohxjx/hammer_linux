cmake_minimum_required(VERSION 3.18)
project(hammer_linux_launcher LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This launcher target is for Linux.")
endif()

set(SOURCE_SDK_ROOT "" CACHE PATH "Source SDK source directory containing public/")
set(SOURCE_SDK_LIBDIR "" CACHE PATH "Directory containing built Source SDK Linux libraries")
if(NOT SOURCE_SDK_ROOT AND DEFINED ENV{SOURCE_SDK_ROOT})
  set(SOURCE_SDK_ROOT "$ENV{SOURCE_SDK_ROOT}")
endif()
if(NOT SOURCE_SDK_LIBDIR AND DEFINED ENV{SOURCE_SDK_LIBDIR})
  set(SOURCE_SDK_LIBDIR "$ENV{SOURCE_SDK_LIBDIR}")
endif()

if(SOURCE_SDK_ROOT AND EXISTS "${SOURCE_SDK_ROOT}/src/public/appframework/AppFramework.h"
   AND NOT EXISTS "${SOURCE_SDK_ROOT}/public/appframework/AppFramework.h")
  set(SOURCE_SDK_ROOT "${SOURCE_SDK_ROOT}/src")
endif()
if(NOT EXISTS "${SOURCE_SDK_ROOT}/public/appframework/AppFramework.h")
  message(FATAL_ERROR "Set SOURCE_SDK_ROOT to the Source SDK tree (must contain public/appframework/AppFramework.h).")
endif()

find_path(HAMMER_INCLUDE_DIR ihammer.h
  PATHS "${SOURCE_SDK_ROOT}" "${SOURCE_SDK_ROOT}/public" "${SOURCE_SDK_ROOT}/hammer"
  NO_DEFAULT_PATH)
find_path(FILESYSTEM_INIT_INCLUDE_DIR filesystem_init.h
  PATHS "${SOURCE_SDK_ROOT}" "${SOURCE_SDK_ROOT}/public" "${SOURCE_SDK_ROOT}/public/tier1"
  NO_DEFAULT_PATH)
if(NOT HAMMER_INCLUDE_DIR OR NOT FILESYSTEM_INIT_INCLUDE_DIR)
  message(FATAL_ERROR "Could not find ihammer.h or filesystem_init.h in SOURCE_SDK_ROOT.")
endif()

set(_lib_hints)
if(SOURCE_SDK_LIBDIR)
  list(APPEND _lib_hints "${SOURCE_SDK_LIBDIR}")
endif()
list(APPEND _lib_hints "${SOURCE_SDK_ROOT}/lib/linux64" "${SOURCE_SDK_ROOT}/lib/linux"
     "${SOURCE_SDK_ROOT}/bin/linux64" "${SOURCE_SDK_ROOT}/bin")
set(_libs)
set(_rpaths)
foreach(name appframework tier0 tier1 vstdlib)
  string(TOUPPER "${name}" upper_name)
  find_library(SDK_${upper_name}_LIBRARY NAMES ${name} HINTS ${_lib_hints})
  if(NOT SDK_${upper_name}_LIBRARY)
    message(FATAL_ERROR "Missing Source SDK library ${name}; build SDK libraries or set SOURCE_SDK_LIBDIR.")
  endif()
  list(APPEND _libs "${SDK_${upper_name}_LIBRARY}")
  get_filename_component(lib_dir "${SDK_${upper_name}_LIBRARY}" DIRECTORY)
  list(APPEND _rpaths "${lib_dir}")
endforeach()
list(REMOVE_DUPLICATES _rpaths)

add_executable(hammer_launcher hammer_launcher/main.cpp)
target_compile_features(hammer_launcher PRIVATE cxx_std_11)
target_compile_definitions(hammer_launcher PRIVATE LINUX POSIX)
target_include_directories(hammer_launcher PRIVATE
  "${SOURCE_SDK_ROOT}/public"
  "${SOURCE_SDK_ROOT}/public/tier0"
  "${SOURCE_SDK_ROOT}/public/tier1"
  "${HAMMER_INCLUDE_DIR}"
  "${FILESYSTEM_INIT_INCLUDE_DIR}")
target_link_libraries(hammer_launcher PRIVATE ${_libs} ${CMAKE_DL_LIBS} pthread m)
set_target_properties(hammer_launcher PROPERTIES
  OUTPUT_NAME hammer
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  BUILD_RPATH "${_rpaths}"
  INSTALL_RPATH "${_rpaths}")
install(TARGETS hammer_launcher RUNTIME DESTINATION bin)
