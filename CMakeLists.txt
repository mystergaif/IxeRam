cmake_minimum_required(VERSION 3.14)
project(LowLevelMemoryDebugger VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch FTXUI
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

include_directories(include)

# Fetch Capstone
FetchContent_Declare(capstone
  GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
  GIT_TAG 5.0.3
)
set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(capstone)

# Fetch Keystone
set(CMAKE_POLICY_DEFAULT_CMP0169 OLD) # Allow FetchContent_Populate for patching
FetchContent_Declare(keystone
  GIT_REPOSITORY https://github.com/keystone-engine/keystone.git
  GIT_TAG 0.9.2
)

FetchContent_GetProperties(keystone)
if(NOT keystone_POPULATED)
  FetchContent_Populate(keystone)
  # Patch Keystone to avoid CMP0051 OLD and VERSION < 3.5 errors on modern CMake
  message(STATUS "Recursively patching Keystone for modern CMake compatibility...")
  # Fix versions in all CMakeLists.txt files
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "CMakeLists.txt" -exec sed -i "s/cmake_minimum_required(VERSION [0-9.]*)/cmake_minimum_required(VERSION 3.5)/g" {} +)
  # Fix policies in all CMakeLists.txt files
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "CMakeLists.txt" -exec sed -i "s/CMP0051 OLD/CMP0051 NEW/g" {} +)
  # Fix missing headers for intptr_t/stdint
  # For C++: Use <cstdint>
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "*.cpp" -exec sed -i "1i#include <cstdint>" {} +)
  # For C: Use <stdint.h>
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "*.c" -exec sed -i "1i#include <stdint.h>" {} +)
  # For Headers: Use <stdint.h> as it works in both C and C++
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "*.h" -exec sed -i "1i#include <stdint.h>" {} +)
  
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(KEYSTONE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(KEYSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)

  # Suppress errors from legacy code in Keystone on modern GCC
  add_definitions(-Wno-error -Wno-deprecated-declarations -Wno-unused-but-set-variable)
  
  add_subdirectory(${keystone_SOURCE_DIR} ${keystone_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_executable(memdebug
    src/main.cpp
    src/Config.cpp
    src/SetupWizard.cpp
    src/MemoryEngine.cpp
    src/Scanner.cpp
    src/TUI.cpp
    src/TUI_run.cpp
)

target_link_libraries(memdebug
    PRIVATE ftxui::screen ftxui::dom ftxui::component
    PRIVATE capstone keystone
    PRIVATE pthread
)
# No need for manual include adjustments as FetchContent targets handle it via INTERFACE_INCLUDE_DIRECTORIES
target_include_directories(memdebug PRIVATE include)

add_library(speedhack SHARED src/speedhack.c)
target_link_libraries(speedhack PRIVATE dl rt)
set_target_properties(speedhack PROPERTIES POSITION_INDEPENDENT_CODE ON)

