cmake_minimum_required(VERSION 3.21)

project(
	NearestQuestTracker
	VERSION 2.0.0
	LANGUAGES CXX
)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

# Static CRT: the DLL must not depend on the user's VC++ redistributable version.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# One DLL for Fallout 4 1.10.163 (OG) and 1.10.984 (NG); VR is not a target.
set(ENABLE_FALLOUT_F4 ON CACHE BOOL "" FORCE)
set(ENABLE_FALLOUT_NG ON CACHE BOOL "" FORCE)
set(ENABLE_FALLOUT_VR OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/CommonLibF4/CommonLibF4 CommonLibF4 EXCLUDE_FROM_ALL)

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
target_link_libraries("${PROJECT_NAME}" PRIVATE CommonLibF4::CommonLibF4 nlohmann_json::nlohmann_json)

if(MSVC)
	target_compile_options("${PROJECT_NAME}" PRIVATE /W4 /utf-8 /Zc:preprocessor)
	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
		"$<$<CONFIG:Release>:/DEBUG:FULL;/OPT:REF;/OPT:ICF>"
	)
endif()
