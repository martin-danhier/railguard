##################################################################
###                            VULKAN                          ###
##################################################################

# Imported in parent directory

##################################################################
###                             SDL2                           ###
##################################################################

if (DEFINED WINDOW_SDL2)
    if (DEFINED CACHE{sdl_include_dir} AND DEFINED CACHE{sdl_lib_dir})
        set(sdl_already_in_cache "true")
    endif ()

    # Find the header directory
    find_path(sdl_include_dir
            "SDL2/SDL.h"
            PATHS "$ENV{SDL2_PATH}/include"
            "/usr/include"
            "$ENV{sdl2_image_DIR}/include"
            )

    # Find the lib directory
    find_path(sdl_lib_dir
            NAMES "libSDL2.so" "SDL2.lib" "SDL2.dll"
            PATHS "$ENV{SDL2_PATH}/lib"
            "/usr/lib/x86_64-linux-gnu/"
            "$ENV{sdl2_image_DIR}/lib"
            "/usr/lib64"
            "/usr/lib64/cmake/SDL2"
            PATH_SUFFIXES ".lib" ".so" ".dll"
            )

    # Not found
    if ((sdl_include_dir STREQUAL "sdl_include_dir-NOTFOUND") OR (sdl_lib_dir STREQUAL "sdl_lib_dir-NOTFOUND"))
        message(FATAL_ERROR "If SDL2 is not located at a conventional place, the root directory needs to be stored in the env variable SDL2_PATH.")
    endif ()

    # Found

    if (NOT sdl_already_in_cache STREQUAL "true")
        message("-- Found SDL2")
        message("       Include dir: ${sdl_include_dir}")
        message("       Lib dir:     ${sdl_lib_dir}")
    endif ()

    # Setup and link SDL
    add_library(sdl2 INTERFACE)
    target_include_directories(sdl2 INTERFACE ${sdl_include_dir})
    target_link_directories(sdl2 INTERFACE ${sdl_lib_dir})
    # Add inside of SDL2 directory to fix "begin_code.h" not found
    target_include_directories(sdl2 INTERFACE "${sdl_include_dir}/SDL2")

    target_link_libraries(sdl2 INTERFACE SDL2)
    if (WIN32)
        target_link_libraries(sdl2 INTERFACE SDL2main)
    endif ()

endif ()

##################################################################
###                       VULKAN-MEM-ALLOC                     ###
##################################################################

if (DEFINED RENDERER_VULKAN)
    add_library(vma STATIC)
    target_sources(vma PRIVATE
            vma_implementation.cpp
            )
    target_include_directories(vma SYSTEM PUBLIC vma/include)
    target_link_libraries(vma PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
endif ()

##################################################################
###                           GLM                              ###
##################################################################

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE glm)


##################################################################
###                             VOLK                           ###
##################################################################

if (DEFINED RENDERER_VULKAN)
    add_library(volk STATIC)
    target_sources(volk PRIVATE
            volk/volk.c
            volk/volk.h
            )
    target_include_directories(volk PUBLIC volk)
    target_link_libraries(volk PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
endif ()

##################################################################
###                      TESTING FRAMEWORK                     ###
##################################################################

add_library(testing_framework STATIC)
target_sources(testing_framework PRIVATE
        testing_framework/test_framework/test_framework.hpp
        testing_framework/src/test_framework.cpp
        testing_framework/test_framework/test_framework.h
        testing_framework/src/test_framework.c
        )
target_include_directories(testing_framework PUBLIC testing_framework)

##################################################################
###                        TINYOBJLOADER                       ###
##################################################################

add_library(tinyobjloader STATIC)
target_sources(tinyobjloader PRIVATE
        tinyobjloader/tiny_obj_loader.h
        tinyobjloader/tiny_obj_loader.cc
        )
target_include_directories(tinyobjloader PUBLIC tinyobjloader)

##################################################################
###                          STB_IMAGE                         ###
##################################################################

add_library(stb_image STATIC)
target_sources(stb_image PRIVATE
        stb_image/stb_image.h
        stb_image/stb_image.cpp
        )
target_include_directories(stb_image PUBLIC stb_image)