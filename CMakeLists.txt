# GluePP: Glue3D converted from Scratch to C++20 + OpenGL 3.3.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release -j
#
# Everything it needs is in third_party/ (GLFW, glad, stb, miniaudio, nlohmann/json); nothing is downloaded.
# The result is build/bin/: Glue3D (the editor), assets/, and player/Glue3DPlayer (what File > build.. copies).
cmake_minimum_required(VERSION 3.20)
project(GluePP LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# One folder with the editor, its assets and the game player, for every build type.
set(OUT_DIR ${CMAKE_BINARY_DIR}/bin)
foreach(cfg "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${cfg} ${OUT_DIR})
endforeach()

# ---- compiler settings ----
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # no extra DLLs needed
  add_compile_options(/utf-8 /bigobj /permissive- /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-ffunction-sections -fdata-sections)
  if(MINGW)
    add_compile_options(-Wa,-mbig-obj)
    # A self-contained .exe: no libstdc++/libgcc/winpthread DLLs next to it.
    add_link_options(-static -static-libgcc -static-libstdc++)
  endif()
  if(APPLE)
    add_link_options(-Wl,-dead_strip)
  else()
    add_link_options(-Wl,--gc-sections)
  endif()
endif()

# ---- third-party libraries (bundled) ----
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE) # X11 on Linux (works under Wayland through XWayland)
add_subdirectory(third_party/glfw EXCLUDE_FROM_ALL)

add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# ---- the Scratch runtime (shared by the editor and the game player) ----
file(GLOB RUNTIME_SOURCES CONFIGURE_DEPENDS runtime/*.cpp)
add_library(scratch_runtime STATIC ${RUNTIME_SOURCES})
target_include_directories(scratch_runtime PUBLIC runtime third_party/stb third_party/miniaudio third_party)
target_link_libraries(scratch_runtime PUBLIC glfw glad)
if(UNIX AND NOT APPLE)
  target_link_libraries(scratch_runtime PUBLIC dl pthread m)
endif()
if(WIN32)
  target_compile_definitions(scratch_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(scratch_runtime PUBLIC comdlg32 shell32) # the "open file" window, Explorer
endif()

# ---- Glue3D's code (converted from Scratch) + hand-written additions ----
file(GLOB GENERATED_SOURCES CONFIGURE_DEPENDS Generated/*.cpp)
set(SHARED_CUSTOM_SOURCES
  Custom/CameraControls.cpp
  Custom/ProjectFiles.cpp
  Custom/PlayerSettings.cpp
  Custom/LoadingScreen.cpp
  Custom/Scrolling.cpp
  Custom/ScriptBlocks.cpp
  Custom/ScriptSystems.cpp
  Custom/ScriptTerminal.cpp
  Custom/ScriptVariables.cpp)
set(EDITOR_CUSTOM_SOURCES
  Custom/MainMenu.cpp
  Custom/SceneGizmo.cpp
  Custom/BuildGame.cpp)
if(WIN32)
  set(APP_RESOURCES app/app.rc)
endif()

# The editor.
add_executable(Glue3D WIN32 app/main.cpp ${GENERATED_SOURCES} ${SHARED_CUSTOM_SOURCES} ${EDITOR_CUSTOM_SOURCES} ${APP_RESOURCES})
target_include_directories(Glue3D PRIVATE Generated Custom)
target_link_libraries(Glue3D PRIVATE scratch_runtime)

# The game player: the same Glue3D code with GLUE3D_PLAYER defined, so BuildFlavor::IsEditor is false and
# every editor-only part is compiled out (and dropped by the linker). File > build.. copies this as <game>.exe.
add_executable(Glue3DPlayer WIN32 Player/PlayerMain.cpp Player/PlayerStubs.cpp ${GENERATED_SOURCES} ${SHARED_CUSTOM_SOURCES} ${APP_RESOURCES})
target_include_directories(Glue3DPlayer PRIVATE Generated Custom)
target_compile_definitions(Glue3DPlayer PRIVATE GLUE3D_PLAYER)
target_link_libraries(Glue3DPlayer PRIVATE scratch_runtime)
set_target_properties(Glue3DPlayer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}/player)
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set_target_properties(Glue3DPlayer PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${cfg} ${OUT_DIR}/player)
endforeach()
add_dependencies(Glue3D Glue3DPlayer) # building the editor always builds the player too
if(MSVC)
  # Windows GUI programs (no console window) that still start at main().
  target_link_options(Glue3D PRIVATE /ENTRY:mainCRTStartup)
  target_link_options(Glue3DPlayer PRIVATE /ENTRY:mainCRTStartup)
endif()

# Strip debug symbols from release builds (smaller game .exe).
if(NOT MSVC AND NOT APPLE)
  target_link_options(Glue3DPlayer PRIVATE $<$<CONFIG:Release,MinSizeRel>:-s>)
  target_link_options(Glue3D PRIVATE $<$<CONFIG:Release,MinSizeRel>:-s>)
endif()

# Costumes, sounds and fonts next to the editor.
add_custom_command(TARGET Glue3D POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Glue3D>/assets
  COMMENT "Copying assets next to Glue3D")
