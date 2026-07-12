cmake_minimum_required(VERSION 3.20)
project(JOJIFrontier CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(FetchContent)

# --- raylib -----------------------------------------------------------
# Prefer an already-installed raylib (e.g. via Homebrew); fall back to
# fetching it so the project builds standalone. When building for the web
# via Emscripten, raylib's own CMake handles the PLATFORM_WEB target.
if(NOT EMSCRIPTEN)
    find_package(raylib 4.5 QUIET)
endif()

if(NOT raylib_FOUND)
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

# --- nlohmann/json (header-only) --------------------------------------
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

# --- game sources -------------------------------------------------------
file(GLOB_RECURSE JF_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)
list(REMOVE_ITEM JF_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)

add_library(jf_lib STATIC ${JF_SOURCES})
target_include_directories(jf_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(jf_lib PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(jf_lib PUBLIC cxx_std_20)

add_executable(JOJIFrontier src/main.cpp)
target_link_libraries(JOJIFrontier PRIVATE jf_lib raylib)
target_include_directories(JOJIFrontier PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(APPLE AND NOT EMSCRIPTEN)
    target_link_libraries(JOJIFrontier PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()

# --- data / assets: copy next to the binary so relative paths resolve --
add_custom_command(TARGET JOJIFrontier POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:JOJIFrontier>/data
)
add_custom_command(TARGET JOJIFrontier POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:JOJIFrontier>/assets
)

# --- Emscripten / WebAssembly build -------------------------------------
# Building with:
#   emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-web
# produces JOJIFrontier.html/.js/.wasm suitable for GitHub Pages hosting.
if(EMSCRIPTEN)
    set_target_properties(JOJIFrontier PROPERTIES SUFFIX ".html")
    target_link_options(JOJIFrontier PRIVATE
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sTOTAL_MEMORY=67108864"
        "--preload-file=${CMAKE_SOURCE_DIR}/data@data"
        "--preload-file=${CMAKE_SOURCE_DIR}/assets@assets"
    )
endif()
