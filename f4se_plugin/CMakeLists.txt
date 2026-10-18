cmake_minimum_required(VERSION 3.21)

project(
	NearestQuestTracker
	VERSION 2.1.0
	LANGUAGES CXX
)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

# Static CRT: the DLL must not depend on the user's VC++ redistributable version.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Two DLLs from one source: 1.10.163 (OG) + 1.10.984 (NG) on alandtse/CommonLibF4, and 1.11.x (AE) on
# libxse/commonlibf4 (AE address IDs differ from NG ones, alandtse has none). VR is not a target.
option(NQT_AE "build for Fallout 4 AE 1.11.x (libxse/commonlibf4)" OFF)
if(NQT_AE)
	add_subdirectory(extern/commonlibf4-ae/lib/commonlib-shared commonlib-shared EXCLUDE_FROM_ALL)
	# libxse builds commonlibf4 with xmake only; same sources and settings as its xmake.lua.
	file(GLOB_RECURSE COMMONLIBF4_AE_SOURCES CONFIGURE_DEPENDS extern/commonlibf4-ae/src/*.cpp)
	add_library(commonlibf4-ae STATIC EXCLUDE_FROM_ALL ${COMMONLIBF4_AE_SOURCES})
	target_include_directories(commonlibf4-ae PUBLIC extern/commonlibf4-ae/include)
	target_link_libraries(commonlibf4-ae PUBLIC commonlib-shared)
	target_precompile_headers(commonlibf4-ae PRIVATE extern/commonlibf4-ae/include/F4SE/Impl/PCH.h)
	if(MSVC)
		target_compile_options(commonlib-shared PUBLIC /EHsc /permissive- /bigobj /utf-8 /Zc:preprocessor /Zc:enumTypes /Zc:templateScope)
	endif()
	set(COMMONLIB_TARGET commonlibf4-ae)
else()
	set(ENABLE_FALLOUT_F4 ON CACHE BOOL "" FORCE)
	set(ENABLE_FALLOUT_NG ON CACHE BOOL "" FORCE)
	set(ENABLE_FALLOUT_VR OFF CACHE BOOL "" FORCE)
	add_subdirectory(extern/CommonLibF4/CommonLibF4 CommonLibF4 EXCLUDE_FROM_ALL)
	set(COMMONLIB_TARGET CommonLibF4::CommonLibF4)
endif()

configure_file(
	cmake/Version.h.in
	"${CMAKE_CURRENT_BINARY_DIR}/include/Version.h"
	@ONLY
)

add_library(
	"${PROJECT_NAME}"
	SHARED
	src/Interiors.cpp
	src/main.cpp
	src/MapMarkers.cpp
	src/Papyrus.cpp
	src/QuestCategories.cpp
	src/QuestDistance.cpp
	src/QuestStarts.cpp
	src/QuestText.cpp
	src/Window.cpp
)

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_23)
target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}/include"
	src
)
target_precompile_headers("${PROJECT_NAME}" PRIVATE src/PCH.h)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries("${PROJECT_NAME}" PRIVATE ${COMMONLIB_TARGET} nlohmann_json::nlohmann_json)
if(NQT_AE)
	target_compile_definitions("${PROJECT_NAME}" PRIVATE NQT_AE=1)
endif()

if(MSVC)
	target_compile_options("${PROJECT_NAME}" PRIVATE /W4 /utf-8 /Zc:preprocessor)
	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
		"$<$<CONFIG:Release>:/DEBUG:FULL;/OPT:REF;/OPT:ICF>"
	)
endif()
