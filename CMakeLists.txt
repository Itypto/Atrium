cmake_minimum_required(VERSION 3.22)

project(Atrium LANGUAGES CXX ASM)

if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "Atrium currently supports Android arm64-v8a only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/dobby/dobby.h" OR
   NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/dobby/${ANDROID_ABI}/libdobby.a")
    message(FATAL_ERROR "Dobby header and Android ${ANDROID_ABI} static library are required in include/dobby")
endif()

add_library(Atrium SHARED
    src/main.cpp
    src/hook.cpp
    src/hud/hud.cpp
    src/setopt_arm64.S
)

target_include_directories(Atrium PRIVATE include)

target_link_options(Atrium PRIVATE "-Wl,-z,max-page-size=16384")

target_link_libraries(Atrium PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/dobby/${ANDROID_ABI}/libdobby.a"
    dl
    log
    android
)
