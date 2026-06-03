cmake_minimum_required(VERSION 3.24)

project(texnologia-polymeswn LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND 
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND 
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}. This project requires GCC or Clang.")
endif()

file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS src/obj/*.hpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_BUILD_TYPE Release)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

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