cmake_minimum_required(VERSION 3.21)

project(ntscreenshot VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(NT_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NT_SOURCE_DIR "${NT_PROJECT_ROOT}/src")
set(NT_THIRD_PARTY_DIR "${NT_SOURCE_DIR}/libs")

list(APPEND CMAKE_MODULE_PATH "${NT_PROJECT_ROOT}/cmake")

include(NtDependencies)
include(NtTarget)

add_executable(ntscreenshot)
add_subdirectory(src)
nt_configure_target(ntscreenshot)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
