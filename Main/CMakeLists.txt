# Main Project

# Find files used for project

set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(INCROOT ${CMAKE_CURRENT_SOURCE_DIR}/include/)
set(PCHROOT ${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB SRC "${SRCROOT}/*.cpp" "${SRCROOT}/*.hpp")
source_group("Source" FILES ${SRC})

file(GLOB INC "${INCROOT}/*.hpp")
source_group("Include" FILES ${INC})

file(GLOB AUDIO_SRC "${SRCROOT}/Audio/*.cpp" "${SRCROOT}/Audio/*.hpp")
source_group("Source\\Audio" FILES ${AUDIO_SRC})

file(GLOB AUDIO_INC "${INCROOT}/Audio/*.hpp")
source_group("Include\\Audio" FILES ${AUDIO_INC})

file(GLOB FAST_GUI_SRC "${SRCROOT}/FastGUI/*.cpp" "${SRCROOT}/FastGUI/*.hpp")
source_group("Source\\FastGUI" FILES ${FAST_GUI_SRC})

file(GLOB FAST_GUI_INC "${INCROOT}/FastGUI/*.hpp")
source_group("Include\\FastGUI" FILES ${FAST_GUI_INC})

file(GLOB NUK_INC "${INCROOT}/nuklear/*.h")
source_group("Include\\nuklear" FILES ${NUK_INC})

file(GLOB NUK_INC "${INCROOT}/nuklear/*.h")
source_group("Include\\nuklear" FILES ${NUK_INC})

file(GLOB LIGHT_INC "${INCROOT}/LightPlugin/*.h")
source_group("Include\\LightPlugin" FILES ${LIGHT_INC})

set(MAIN_SRC ${SRC} ${INC} ${AUDIO_SRC} ${AUDIO_INC} ${FAST_GUI_SRC} ${FAST_GUI_INC} ${NUK_INC} ${LIGHT_INC})

file(GLOB RESOURCE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Resource.rc)
source_group("" FILES ${RESOURCE_SRC})

set(PCH_SRC ${PCHROOT}/stdafx.cpp)
set(PCH_INC ${PCHROOT}/stdafx.h)
set(PCH_FILES ${PCH_SRC} ${PCH_INC})
source_group("" FILES ${PCH_FILES})

# Compiler stuff
enable_precompiled_headers("${MAIN_SRC}" ${PCH_SRC})

include_directories(Audio GUI .)
add_executable(einherjarSDVX ${MAIN_SRC} ${RESOURCE_SRC} ${PCH_FILES})
target_include_directories(einherjarSDVX PRIVATE
	${SRCROOT}
	${SRCROOT}/GUI
	${SRCROOT}/Audio
	${INCROOT}
	${INCROOT}/GUI
	${INCROOT}/Audio
)
target_compile_features(einherjarSDVX PUBLIC cxx_std_17)
target_compile_definitions(einherjarSDVX PRIVATE VERSION_MINOR=${PROJECT_VERSION_MINOR})
target_compile_definitions(einherjarSDVX PRIVATE VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
target_compile_definitions(einherjarSDVX PRIVATE VERSION_PATCH=${PROJECT_VERSION_PATCH})
find_package (Git)
if(GIT_FOUND)
	target_compile_definitions(einherjarSDVX PRIVATE GIT_COMMIT=${GIT_DATE_HASH})
	execute_process(
	  COMMAND git rev-parse --abbrev-ref HEAD
	  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	  OUTPUT_VARIABLE GIT_BRANCH
	  OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	target_compile_definitions(einherjarSDVX PRIVATE GIT_BRANCH="${GIT_BRANCH}")
else(GIT_FOUND)
	target_compile_definitions(einherjarSDVX PRIVATE GIT_COMMIT="")
	target_compile_definitions(einherjarSDVX PRIVATE GIT_BRANCH="master")
endif(GIT_FOUND)

set_output_postfixes(einherjarSDVX)
set_werror(einherjarSDVX)

if(MSVC)
	# Set debugging folder
	set_target_properties(einherjarSDVX PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
	target_compile_options(einherjarSDVX PRIVATE /EHsc /MP)
	# Target subsystem on windows, enable multiprocess build and faster PDB gen
	set_target_properties(einherjarSDVX PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:WINDOWS /DEBUG:FASTLINK")
	set_target_properties(einherjarSDVX PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /DEBUG:FULL")
endif(MSVC)

# Dependencies
target_link_libraries(einherjarSDVX Shared)
target_link_libraries(einherjarSDVX Graphics)
target_link_libraries(einherjarSDVX Audio)
target_link_libraries(einherjarSDVX GUI)
target_link_libraries(einherjarSDVX Beatmap)
target_link_libraries(einherjarSDVX nanovg)
target_link_libraries(einherjarSDVX lua)
target_link_libraries(einherjarSDVX discord-rpc)
target_link_libraries(einherjarSDVX cpr)
target_link_libraries(einherjarSDVX nlohmann_json)

target_link_libraries(einherjarSDVX ${SDL2_LIBRARY})
target_link_libraries(einherjarSDVX ${LibArchive_LIBRARIES})
target_include_directories(einherjarSDVX SYSTEM PRIVATE ${LibArchive_INCLUDE_DIRS})

if(WIN32)
	target_link_libraries(einherjarSDVX
		optimized ${PROJECT_SOURCE_DIR}/third_party/breakpad/exception_handler_Release.lib
		optimized ${PROJECT_SOURCE_DIR}/third_party/breakpad/crash_generation_client_Release.lib
		optimized ${PROJECT_SOURCE_DIR}/third_party/breakpad/common_Release.lib
	)
	target_link_libraries(einherjarSDVX
		debug ${PROJECT_SOURCE_DIR}/third_party/breakpad/exception_handler_Debug.lib
		debug ${PROJECT_SOURCE_DIR}/third_party/breakpad/crash_generation_client_Debug.lib
		debug ${PROJECT_SOURCE_DIR}/third_party/breakpad/common_Debug.lib
	)
	target_include_directories(einherjarSDVX PUBLIC ${PROJECT_SOURCE_DIR}/third_party/breakpad/include)
endif()

find_package(ZLIB REQUIRED)
if(ZLIB_FOUND)
	target_compile_definitions(einherjarSDVX PRIVATE ZLIB_FOUND=${ZLIB_FOUND})
endif()

target_link_libraries(einherjarSDVX cc-common)
