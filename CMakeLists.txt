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

include(CTest)
if(BUILD_TESTING)
    add_executable(jf_battle_tests tests/test_battle.cpp)
    target_link_libraries(jf_battle_tests PRIVATE jf_lib)
    target_compile_definitions(jf_battle_tests PRIVATE JF_SOURCE_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
    add_test(NAME jf_battle_tests COMMAND jf_battle_tests)
    # The default build type (RelWithDebInfo, see above) defines NDEBUG,
    # which compiles every assert() in test_battle.cpp into a no-op -
    # including asserts that both perform an action and check its result
    # (e.g. assert(app.startExpedition())), so entire setup steps would
    # silently never run. Force assertions on for this target regardless of
    # build type so the test suite actually verifies (and executes) anything.
    if(MSVC)
        target_compile_options(jf_battle_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(jf_battle_tests PRIVATE -UNDEBUG)
    endif()

    add_executable(jf_forest_balance tools/forest_balance.cpp)
    target_link_libraries(jf_forest_balance PRIVATE jf_lib)

    add_executable(jf_class_duel tools/class_duel.cpp)
    target_link_libraries(jf_class_duel PRIVATE jf_lib)
endif()

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
        "-lidbfs.js"
        "-sTOTAL_MEMORY=67108864"
        "--pre-js=${CMAKE_SOURCE_DIR}/web/idbfs.js"
        "--preload-file=${CMAKE_SOURCE_DIR}/data@data"
        "--preload-file=${CMAKE_SOURCE_DIR}/assets@assets"
    )
endif()
