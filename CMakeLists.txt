# Config based on https://github.com/krux02/minimal_cmake_example

cmake_minimum_required(VERSION 3.20)
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

# Enable/disable sanitizers
set(enable_asan  1)
set(enable_ubsan 1)

# Define C and C++ versions
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 20)

# Enable tests
enable_testing()

##################################################################
###                          SOURCES                           ###
##################################################################

# Add source files for main lib
set(railguard_lib_sources
    src/core/engine.cpp
    src/core/window/window_sdl2.cpp
    src/utils/vector_impl.cpp
        src/utils/hash_map.cpp
    include/railguard/utils/array.h
    include/railguard/utils/vector.h
    include/railguard/utils/optional.h
        )

# Add header directories for main lib
set(railguard_lib_header_dir
    include
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
foreach(flag ${settings})
    add_compile_definitions(${flag})
    set(${flag} "true")
endforeach()

# Vulkan specific
if (DEFINED RENDERER_VULKAN)
    add_compile_definitions(VK_NO_PROTOTYPE)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(
            USE_VK_VALIDATION_LAYERS
        )
    endif()
endif()

# Enable sanitizers if variable is 1
if (${enable_asan} STREQUAL "1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

if (${enable_ubsan} STREQUAL "1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
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

# Macro inspired by https://bertvandenbroucke.netlify.app/2019/12/12/unit-testing-with-ctest/

# Add a new unit test
# A new target with the test sources is constructed, and a CTest test with the
# same name is created. The new test is also added to the global list of test
# contained in the check target
macro(add_unit_test)
    # Define macro arguments
    set(options PARALLEL)
    set(oneValueArgs FILE)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "" ${ARGN})

    # Remove extension of TEST_FILE.
    set(TEST_NAME ${TEST_FILE})
    string(REGEX REPLACE ".cpp$" "" TEST_NAME ${TEST_NAME})
    # Also replace \ by /
    string(REGEX REPLACE "/" "-" TEST_NAME ${TEST_NAME})


    # Compile test
    message(STATUS "Generating test \"${TEST_NAME}\"")
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL tests/${TEST_FILE})
    target_sources(${TEST_NAME} PRIVATE tests/${TEST_FILE})

    # Set directory for executable
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    # Link project lib and testing framework
    target_link_libraries(${TEST_NAME} railguard_lib testing_framework)

    # Register test
    add_test(NAME ${TEST_NAME}
             WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/tests
             COMMAND ${TEST_NAME})

    # Add test to list
    set(TEST_NAMES ${TEST_NAMES} ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES
        ENVIRONMENT "TEST_FILE=tests/${TEST_FILE};TEST_LINE=0"
    )
endmacro(add_unit_test)

# Get all c++ files in the tests directory, recursively
file(GLOB_RECURSE test_files
        "tests/*.cpp"
    )

# For each one, add it
foreach(test_file ${test_files})
    # Get local path (remove prefix project source)
    string(REGEX REPLACE "^${PROJECT_SOURCE_DIR}/tests/" "" test_file ${test_file})

    add_unit_test(FILE ${test_file})
endforeach(test_file)

# Save target
add_custom_target(
    tests
    DEPENDS ${TEST_NAMES}
)


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