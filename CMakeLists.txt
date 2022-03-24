# Config based on https://github.com/krux02/minimal_cmake_example

cmake_minimum_required(VERSION 3.17)
project(railguard)

##################################################################
###                          SETTINGS                          ###
##################################################################

# Add settings flags here
set(settings

    # Window framework: one of {WINDOW_SDL2}
    WINDOW_SDL2

    # Rendering framework: one of {RENDERER_VULKAN}
    RENDERER_VULKAN
)

# Define C and C++ versions
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 11)

##################################################################
###                          SOURCES                           ###
##################################################################

# Add source files for main lib
set(railguard_lib_sources
    src/core/engine.cpp
)

# Add header directories for main lib
set(railguard_lib_header_dir
    include
)

# Add source files for tests
set(test_sources
    tests/main.cpp
)

# Add resource files for tests
set(test_resources

)

##################################################################
###                       APPLY SETTINGS                       ###
##################################################################

# Don't change this directly

# Debug vs Release
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(
        DEBUG
        MEMORY_CHECKS
    )
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(
        NDEBUG
    )
endif ()

# Add dependencies flags
add_compile_definitions(${settings})

# Vulkan specific
if (RENDERER_VULKAN IN_LIST settings)

    add_compile_definitions(VK_NO_PROTOTYPE)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(
            USE_VK_VALIDATION_LAYERS
        )
    endif()
endif()

##################################################################
###                         CREATE LIB                         ###
##################################################################

# Create main lib
add_library               (railguard_lib STATIC  ${railguard_lib_sources})
target_sources            (railguard_lib PRIVATE ${railguard_lib_sources})
target_include_directories(railguard_lib PUBLIC  ${railguard_lib_header_dir})

##################################################################
###                         DEPENDENCIES                       ###
##################################################################

# Find Vulkan if needed
if (DEFINED RENDERER_VULKAN)
    find_package(Vulkan REQUIRED)
endif()

# Load external script
add_subdirectory(external)

# Link dependencies
if (DEFINED RENDERER_VULKAN)
    target_link_libraries(railguard_lib PUBLIC
        Vulkan::Vulkan
        volk
    )
endif()

if (DEFINED WINDOW_SDL2)
    target_link_libraries(railguard_lib PUBLIC
        sdl2
    )
endif()

##################################################################
###                           TESTING                          ###
##################################################################

# Create test executable
add_executable (tests ${test_resources})
target_sources (tests PRIVATE ${test_sources})

# Link original executable as lib
target_link_libraries(tests PUBLIC railguard_lib)

# Definition to recognize tests from inside the code
target_compile_definitions(tests PUBLIC UNIT_TESTS)

##################################################################
###                     SHADER COMPILATION                     ###
##################################################################

# Shaders need to be compiled in the renderering engine is Vulkan
if (DEFINED RENDERER_VULKAN)

    # Get compiler
    find_program(glsl_validator glslangValidator HINTS
        /usr/bin
        /usr/local/bin
        $ENV{VULKAN_SDK}/Bin/
        $ENV{VULKAN_SDK}/Bin32/
        ${PROJECT_SOURCE_DIR}/glslangValidator/bin/ # For GitHub Actions
    )

    # Find shader files
    file(GLOB_RECURSE glsl_source_files
        "${PROJECT_SOURCE_DIR}/resources/shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/resources/shaders/*.comp"
    )

    # Iterate each shader
    foreach(glsl ${glsl_source_files})
        message(STATUS "Building shader")
        get_filename_component(file_name ${glsl} NAME)
        set(spirv "${CMAKE_BINARY_DIR}/resources/shaders/${file_name}.spv")
        message(STATUS ${glsl})

        # Execute glslang command to compile that specific shader
        add_custom_command(
                OUTPUT ${spirv}
                COMMAND ${glsl_validator} -V ${glsl} -o ${spirv}
                DEPENDS ${glsl})
        list(APPEND spirv_binary_files ${spirv})
    endforeach(glsl)

    # Link it to the main lib
    add_custom_target(shaders DEPENDS ${spirv_binary_files})
    add_dependencies(railguard_lib shaders)
endif()