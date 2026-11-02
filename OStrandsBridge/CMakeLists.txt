cmake_minimum_required(VERSION 3.24)

project(PPB_OStrands VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
# Same CRT as OStrands (/MD). The bridge never relies on it for memory it hands over (that goes through ucrtbase
# directly, see OStrandsLink.cpp), but matching avoids surprises.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if(NOT MSVC)
	message(FATAL_ERROR "Build with MSVC or clang-cl: the OStrands call passes std::vector by value and relies on the MSVC ABI.")
endif()

# CommonLibVR (the maintained NG fork: one binary for SE, AE and VR). Pin COMMONLIB_TAG to a commit for
# reproducible builds; the default follows the ng branch.
set(COMMONLIB_TAG "ng" CACHE STRING "alandtse/CommonLibVR git ref")
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
	CommonLibSSE
	GIT_REPOSITORY https://github.com/alandtse/CommonLibVR.git
	GIT_TAG ${COMMONLIB_TAG}
	GIT_SHALLOW ON
	GIT_SUBMODULES_RECURSE ON
)
FetchContent_MakeAvailable(CommonLibSSE)

# CommonLibVR only includes its plugin helper itself when it links a prebuilt bundle; on a source build (e.g. the
# bundle was made with a different MSVC toolset) the consumer has to pull add_commonlibsse_plugin() in.
if(NOT COMMAND add_commonlibsse_plugin)
	include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")
endif()

add_commonlibsse_plugin(
	${PROJECT_NAME}
	NAME "PPB_OStrands"
	AUTHOR "PPB OStrands Bridge"
	SOURCES
		src/main.cpp
		src/Settings.cpp
		src/OStrandsLink.cpp
		src/Body.cpp
		src/StrokeMeter.cpp
		src/PathDecals.cpp
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8 /Zc:preprocessor)
# Release / RelWithDebInfo only: Debug builds add an iterator-debugging pointer to std::vector, which changes its
# size - OStrandsLink.cpp static_asserts that and refuses to compile rather than corrupt OStrands' queue.

# `cmake --install build --prefix <mod folder>` lays the DLL out like the package.
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "SKSE/Plugins")
