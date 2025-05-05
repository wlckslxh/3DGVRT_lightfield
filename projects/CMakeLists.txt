# Copyright (c) 2016-2024, Sascha Willems
# SPDX-License-Identifier: MIT
# Sogang University, Graphics Lab

# Function for building single project
function(buildProject PROJECT_NAME)
	SET(PROJECT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME})
	message(STATUS "Generating project file for single project in ${PROJECT_FOLDER}")
	# Main
	file(GLOB SOURCE *.cpp ${BASE_HEADERS} ${PROJECT_FOLDER}/*.cpp)
	SET(MAIN_CPP ${PROJECT_FOLDER}/${PROJECT_NAME}.cpp)	
	if(EXISTS ${PROJECT_FOLDER}/main.cpp)
		SET(MAIN_CPP ${PROJECT_FOLDER}/main.cpp)
	ENDIF()
	file(GLOB HEADER ${PROJECT_FOLDER}/*.hpp ${PROJECT_FOLDER}/*.h)
	if(EXISTS ${PROJECT_FOLDER}/${PROJECT_NAME}.h)
		SET(MAIN_HEADER ${PROJECT_FOLDER}/${PROJECT_NAME}.h)
	ENDIF()
	# imgui example requires additional source files
	IF(${PROJECT_NAME} STREQUAL "imgui")
		file(GLOB ADD_SOURCE "../external/imgui/*.cpp")
		SET(SOURCE ${SOURCE} ${ADD_SOURCE})
	ENDIF()
	# wayland requires additional source files
	IF(USE_WAYLAND_WSI)
		SET(SOURCE ${SOURCE} ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h ${CMAKE_BINARY_DIR}/xdg-shell-protocol.c)
	ENDIF()
	# Add shaders
	set(SHADER_DIR_GLSL "../shaders/glsl/${PROJECT_NAME}")
	file(GLOB SHADERS_GLSL "${SHADER_DIR_GLSL}/*.vert" "${SHADER_DIR_GLSL}/*.frag" "${SHADER_DIR_GLSL}/*.comp" "${SHADER_DIR_GLSL}/*.geom" "${SHADER_DIR_GLSL}/*.tesc" "${SHADER_DIR_GLSL}/*.tese" "${SHADER_DIR_GLSL}/*.mesh" "${SHADER_DIR_GLSL}/*.task" "${SHADER_DIR_GLSL}/*.rgen" "${SHADER_DIR_GLSL}/*.rchit" "${SHADER_DIR_GLSL}/*.rmiss" "${SHADER_DIR_GLSL}/*.rcall" "${SHADER_DIR_GLSL}/*.rahit" "${SHADER_DIR_GLSL}/*.rint" "${SHADER_DIR_GLSL}/*.glsl")
	source_group("Shaders" FILES ${SHADERS_GLSL})

	# Add optional readme / tutorial
	file(GLOB README_FILES "${PROJECT_FOLDER}/*.md")
	if(WIN32)
		add_executable(${PROJECT_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${HEADER} ${SHADERS_GLSL} ${README_FILES})
		target_link_libraries(${PROJECT_NAME} base ${Vulkan_LIBRARY} ${WINLIBS} ${KTX_LIBRARY})
	else(WIN32)
		add_executable(${PROJECT_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${HEADER} ${SHADERS_GLSL} ${README_FILES})
		target_link_libraries(${PROJECT_NAME} base )
	endif(WIN32)

	set_property(TARGET ${PROJECT_NAME} PROPERTY 
		VS_DEBUGGER_ENVIRONMENT "PATH=$ENV{PATH}"
	)

	file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
	set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
	if(${PROJECT_NAME} STREQUAL "texture3d")
		if(APPLE)
			# SRS - Use MacPorts paths as default since the same on x86 and Apple Silicon, can override for homebrew on cmake command line
			if(NOT OpenMP_omp_LIBRARY AND EXISTS /opt/local/lib/libomp/libomp.dylib)
				set(OpenMP_omp_LIBRARY /opt/local/lib/libomp/libomp.dylib)
			endif()
			if(CMAKE_C_COMPILER_ID MATCHES "Clang\$")
				set(OpenMP_C_FLAGS "-Xclang -fopenmp")
				set(OpenMP_C_LIB_NAMES "omp")
				if(NOT OpenMP_C_INCLUDE_DIR AND EXISTS /opt/local/include/libomp)
					set(OpenMP_C_INCLUDE_DIR /opt/local/include/libomp)
				endif()
			endif()
			if(CMAKE_CXX_COMPILER_ID MATCHES "Clang\$")
				set(OpenMP_CXX_FLAGS "-Xclang -fopenmp")
				set(OpenMP_CXX_LIB_NAMES "omp")
				if(NOT OpenMP_CXX_INCLUDE_DIR AND EXISTS /opt/local/include/libomp)
					set(OpenMP_CXX_INCLUDE_DIR /opt/local/include/libomp)
				endif()
			endif()
		endif()
		find_package(OpenMP)
		if(OpenMP_CXX_FOUND)
			link_directories(${OpenMP_CXX_LIBRARY_DIRS})
		endif()
	endif()

	if(RESOURCE_INSTALL_DIR)
		install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endfunction(buildProject)

# Build all projects
function(buildProjects)
	foreach(PROJECT ${PROJECTS})
		buildProject(${PROJECT})
	endforeach(PROJECT)
endfunction(buildProjects)

set(PROJECTS
	VulkanFullRT
	VulkanHybrid

buildProjects()
