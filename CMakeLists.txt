cmake_minimum_required(VERSION 3.24)

project(texnologia-polymeswn LANGUAGES CXX)

file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS src/obj/*.hpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 1. Change default build type to Release (Debug disables optimizations!)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 2. Explicitly add -O3 optimizations based on the compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -O3
        -march=native     # Target the specific CPU of the machine building the code
        -ffast-math       # Breaks some IEEE standards for 2x speed in physics
        -flto             # Link time optimization
        -funroll-loops    # Aggressive loop unrolling
        -fomit-frame-pointer 
        -fstrict-aliasing
        -Rpass=loop-vectorize # Keep this to see optimization reports
    )
    # Ensure the linker also uses LTO
    target_link_options(${PROJECT_NAME} PRIVATE -flto)
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /O2) # MSVC equivalent of -O3
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)

set(BASE_ASSET_DIR "${CMAKE_SOURCE_DIR}/assets")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ASSET_PATH=\"${BASE_ASSET_DIR}\"
)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # Don't build raylib examples
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE) # Don't build raylib games

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
FetchContent_MakeAvailable(raylib)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(APPLE)
    # Required Frameworks for macOS
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework CoreVideo"
        "-framework IOKit"
        "-framework Cocoa"
        "-framework GLUT"
        "-framework OpenGL"
    )
elseif(LINUX)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        m pthread dl rt X11
    )
endif()