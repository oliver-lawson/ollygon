cmake_minimum_required(VERSION 3.16)

project(ollygon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# add cmake module path for FindOptiX
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ============================================================================
# Find Required Dependencies
# ============================================================================

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Widgets OpenGLWidgets)
find_package(OpenGL REQUIRED)

# ============================================================================
# CUDA & OptiX Configuration (optional - for GPU rendering)
# ============================================================================

# configure these paths for your system.  I'm using latest as of 2025-11-18
set(CUDA_TOOLKIT_ROOT_DIR "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.0" 
    CACHE PATH "CUDA Toolkit directory")
set(OptiX_INSTALL_DIR "C:/Program Data/NVIDIA Corporation/OptiX SDK 9.0.0" 
    CACHE PATH "OptiX SDK installation directory")

# set CUDA paths for CMake and VS
set(ENV{CUDA_PATH} "${CUDA_TOOLKIT_ROOT_DIR}")
set(CMAKE_CUDA_COMPILER "${CUDA_TOOLKIT_ROOT_DIR}/bin/nvcc.exe")
set(CMAKE_VS_GLOBALS "CudaToolkitDir=${CUDA_TOOLKIT_ROOT_DIR}")
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_VS_GLOBALS)

# attempt to find CUDA
if(EXISTS "${CMAKE_CUDA_COMPILER}")
    set(CUDA_FOUND TRUE)
    message(STATUS "found CUDA: ${CMAKE_CUDA_COMPILER}")
    message(STATUS "CUDA toolkit: ${CUDA_TOOLKIT_ROOT_DIR}")
    
    enable_language(CUDA)
    set(CMAKE_CUDA_ARCHITECTURES "89;86;80;75")
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    set(CUDA_INCLUDE_DIRS "${CUDA_TOOLKIT_ROOT_DIR}/include")
    
    # search for OptiX
    find_path(OptiX_INCLUDE_DIR
        NAMES optix.h
        HINTS
            ${OptiX_INSTALL_DIR}
            "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0"
            "C:/ProgramData/NVIDIA Corporation/OptiX SDK 8.0.0"
            "C:/Program Files/NVIDIA Corporation/OptiX SDK 9.0.0"
            "/usr/local/optix"
            "/opt/optix"
        PATH_SUFFIXES include
        DOC "OptiX include directory"
    )
    
    if(OptiX_INCLUDE_DIR)
        set(OptiX_FOUND TRUE)
        
        # find CUDA libraries
        find_library(CUDA_CUDA_LIBRARY
            NAMES cuda
            HINTS "${CUDA_TOOLKIT_ROOT_DIR}"
            PATH_SUFFIXES lib/x64 lib64 lib
        )
        
        find_library(CUDA_CUDART_LIBRARY
            NAMES cudart
            HINTS "${CUDA_TOOLKIT_ROOT_DIR}"
            PATH_SUFFIXES lib/x64 lib64 lib
        )
        
        message(STATUS "found OptiX!: ${OptiX_INCLUDE_DIR}")
        message(STATUS "CUDA /include/: ${CUDA_INCLUDE_DIRS}")
        message(STATUS "CUDA driver /lib/: ${CUDA_CUDA_LIBRARY}")
        message(STATUS "CUDA runtime /lib/: ${CUDA_CUDART_LIBRARY}")
        add_definitions(-DOLLYGON_USE_OPTIX)
        message(STATUS "building with OptiX support")
    else()
        message(STATUS "OptiX not found.  edit OptiX_INSTALL_DIR in CMakeLists.txt to enable GPU rendering!")
        message(STATUS "building without OptiX (CPU rendering only)")
    endif()
else()
    set(CUDA_FOUND FALSE)
    set(OptiX_FOUND FALSE)
    message(STATUS "CUDA not found at: ${CMAKE_CUDA_COMPILER}")
    message(STATUS "edit CUDA_TOOLKIT_ROOT_DIR in CMakeLists.txt if you have CUDA installed.")
    message(STATUS "building without OptiX (CPU raytracing only)")
endif()

# ============================================================================
# Source Files
#
# auto globbed so I don't have have to manually add every file
# ============================================================================

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.hpp")

# ============================================================================
# Create Executable
# ============================================================================

add_executable(ollygon ${SOURCES} ${HEADERS})

# ============================================================================
# OptiX .ptx Compilation
# ============================================================================

if(OptiX_FOUND)
    set_target_properties(ollygon PROPERTIES
        CUDA_ARCHITECTURES "75;80;86;89"
    )
    
    # compile CUDA kernels to ptx
    set(OPTIX_KERNELS_CU "${CMAKE_CURRENT_SOURCE_DIR}/src/okaytracer/optix_kernels.cu")
    set(OPTIX_KERNELS_PTX "${CMAKE_CURRENT_BINARY_DIR}/optix_kernels.ptx")
    
    add_custom_command(
        OUTPUT ${OPTIX_KERNELS_PTX}
        COMMAND ${CMAKE_CUDA_COMPILER} 
            -ptx
            "${OPTIX_KERNELS_CU}"
            -I"${OptiX_INCLUDE_DIR}"
            -I"${CUDA_INCLUDE_DIRS}"
            -I"${CMAKE_CURRENT_SOURCE_DIR}/src"
            -I"${CMAKE_CURRENT_SOURCE_DIR}/external"
            -std=c++17
            --generate-code=arch=compute_89,code=compute_89
            -DOLLYGON_USE_OPTIX
            -o "${OPTIX_KERNELS_PTX}"
        DEPENDS ${OPTIX_KERNELS_CU}
        COMMENT "compiling OptiX kernels to PTX"
        VERBATIM
    )
    
    # copy .ptx to executable directory, this should work from both the .exe and launching from VS
    add_custom_command(TARGET ollygon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPTIX_KERNELS_PTX}"
            "$<TARGET_FILE_DIR:ollygon>/optix_kernels.ptx"
        COMMENT "copying OptiX .ptx to executable directory"
    )
    
    add_custom_target(optix_ptx ALL DEPENDS ${OPTIX_KERNELS_PTX})
    add_dependencies(ollygon optix_ptx)
endif()

# ============================================================================
# Include Directories
# ============================================================================

target_include_directories(ollygon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

if(OptiX_FOUND)
    target_include_directories(ollygon PRIVATE
        ${OptiX_INCLUDE_DIR}
        ${CUDA_INCLUDE_DIRS}
    )
endif()

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(ollygon PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGLWidgets
    OpenGL::GL
)

if(OptiX_FOUND)
    target_link_libraries(ollygon PRIVATE
        ${CUDA_CUDA_LIBRARY}
        ${CUDA_CUDART_LIBRARY}
    )
endif()

# ============================================================================
# Post-Build: Copy Assets
# ============================================================================

add_custom_command(TARGET ollygon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:ollygon>/assets
    COMMENT "copying assets to build directory"
)

# ============================================================================
# Platform-Specific Settings
# ============================================================================

if(WIN32)
    set_target_properties(ollygon PROPERTIES
        WIN32_EXECUTABLE $<$<CONFIG:Release>:ON>  # console window only in debug builds
    )
endif()

# ============================================================================
# Visual Studio Settings
# ============================================================================

if(MSVC)
    # set as startup project
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ollygon)
    
    # hot reload in debug builds
    target_compile_options(ollygon PRIVATE
        $<$<CONFIG:Debug>:/ZI>      # edit and continue debug info
        $<$<CONFIG:Debug>:/Ob0>     # disable inlining for debug
    )
    target_link_options(ollygon PRIVATE
        $<$<CONFIG:Debug>:/INCREMENTAL>  # incremental linking
    )
    
    # debugger working directory
    set_target_properties(ollygon PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    
    # hide MOC autogen files from solution explorer
    source_group("CMake Autogen" REGULAR_EXPRESSION ".*_autogen/.*")
    set_source_files_properties(
        ${CMAKE_CURRENT_BINARY_DIR}/ollygon_autogen/mocs_compilation.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE
    )
endif()