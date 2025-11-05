cmake_minimum_required(VERSION 3.16)

project(ollygon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# grab Qt
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Widgets OpenGLWidgets)

# grab openGL
find_package(OpenGL REQUIRED)

# collect all source/headers
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS 
    "src/*.hpp"
)

# executable
add_executable(ollygon ${SOURCES} ${HEADERS})

# include directories
target_include_directories(ollygon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# link libraries
target_link_libraries(ollygon PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGLWidgets
    OpenGL::GL
)

# copy assets to build directory
add_custom_command(TARGET ollygon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:ollygon>/assets
)

# platform-specific settings
if(WIN32)
    set_target_properties(ollygon PROPERTIES
        WIN32_EXECUTABLE $<$<CONFIG:Release>:ON>  # console window only in debug builds
    )
endif()

# Visual Studio specific settings
if(MSVC)
    # set as startup project
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ollygon)
    
    # hot reload support
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