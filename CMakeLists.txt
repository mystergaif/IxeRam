cmake_minimum_required(VERSION 3.14)
project(LowLevelMemoryDebugger VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Content for Dependencies
include(FetchContent)

# 1. FTXUI for TUI support
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# 2. Capstone for disassembly
FetchContent_Declare(capstone
  GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
  GIT_TAG 5.0.3
)
set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(capstone)

# 3. Keystone for assembly
set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)
FetchContent_Declare(keystone
  GIT_REPOSITORY https://github.com/keystone-engine/keystone.git
  GIT_TAG 0.9.2
)

FetchContent_GetProperties(keystone)
if(NOT keystone_POPULATED)
  FetchContent_Populate(keystone)
  # Patch Keystone for modern CMake compatibility
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "CMakeLists.txt" -exec sed -i "s/cmake_minimum_required(VERSION [0-9.]*)/cmake_minimum_required(VERSION 3.5)/g" {} +)
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "CMakeLists.txt" -exec sed -i "s/CMP0051 OLD/CMP0051 NEW/g" {} +)
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "*.cpp" -exec sed -i "1i#include <cstdint>" {} +)
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "*.c" -exec sed -i "1i#include <stdint.h>" {} +)
  execute_process(COMMAND find ${keystone_SOURCE_DIR} -name "*.h" -exec sed -i "1i#include <stdint.h>" {} +)
  
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(KEYSTONE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(KEYSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  add_definitions(-Wno-error -Wno-deprecated-declarations -Wno-unused-but-set-variable)
  add_subdirectory(${keystone_SOURCE_DIR} ${keystone_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# 4. Qt 6 for GUI support
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Main Executable: ixeram (Combined TUI and GUI)
add_executable(ixeram
    src/main.cpp
    src/Config.cpp
    src/SetupWizard.cpp
    src/MemoryEngine.cpp
    src/Scanner.cpp
    src/TUI.cpp
    src/TUI_run.cpp
    src/gui/MainWindow.cpp
    include/MainWindow.hpp
    src/gui/ProcessSelectorDialog.cpp
    include/ProcessSelectorDialog.hpp
)

target_link_libraries(ixeram
    PRIVATE ftxui::screen ftxui::dom ftxui::component
    PRIVATE Qt6::Widgets
    PRIVATE capstone keystone
    PRIVATE pthread dl
)

target_include_directories(ixeram PRIVATE include)

# Speedhack library
add_library(speedhack SHARED src/speedhack.c)
target_link_libraries(speedhack PRIVATE dl rt)
set_target_properties(speedhack PROPERTIES POSITION_INDEPENDENT_CODE ON)
