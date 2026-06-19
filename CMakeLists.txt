# cmake minimum version req
cmake_minimum_required(VERSION 3.12)

# using MSVC compiler
if(MSVC)
	# static runtime
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# project (using C++)
project(Scorpio LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

# GLFW no build, install
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# pull dependecy cmake lists
add_subdirectory(deps/glfw-3.4)				# window opener
add_subdirectory(deps/glad)				# opengl loader
add_subdirectory(deps/glm-1.01)				# math
add_subdirectory(deps/stb)				# stb image
add_subdirectory(deps/imgui-1.92.5)			# dear imgui
add_subdirectory(deps/libnoise)				# libnoise

# assimp
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)

add_subdirectory(deps/assimp-6.0.5)

if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})
endif()

# collect all *.cpp under src/ into MY_SOURCES
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")


# create exe target
add_executable("${CMAKE_PROJECT_NAME}")
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES})

if (MSVC)
    target_compile_options("${CMAKE_PROJECT_NAME}" PRIVATE /MP)
else()
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# include header directories
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# define preprocessor macro
target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC 
	RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/res/"
	SAVE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/world/"
	VULKAN_HPP_NO_EXCEPTIONS
	VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

# link libs
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE
	d3d12
	dxgi
	dxguid
	Vulkan::Vulkan
	Threads::Threads
	glm 
	glfw 
	glad 
	stb 
	imgui 
	libnoise 
	assimp
)
