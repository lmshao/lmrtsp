cmake_minimum_required(VERSION 3.10)
project(rtsp VERSION 0.0.1 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_STATIC_LIBS "Build static libraries" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_APPS "Build RTSP applications (server/client)" ON)
option(INSTALL_TO_USER_LOCAL "Install to ~/.local instead of system-wide" OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Display configuration options for developers
message(STATUS "")
message(STATUS "========== RTSP Library Configuration Options ==========")
message(STATUS "")
message(STATUS "Build Type Configuration:")
message(STATUS "  Current build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Available types: Debug, Release, RelWithDebInfo, MinSizeRel")
message(STATUS "  Change with: cmake -DCMAKE_BUILD_TYPE=Debug ..")
message(STATUS "")
message(STATUS "Library Build Options:")
message(STATUS "  BUILD_STATIC_LIBS: Build static libraries (current: ${BUILD_STATIC_LIBS})")
message(STATUS "  BUILD_SHARED_LIBS: Build shared libraries (current: ${BUILD_SHARED_LIBS})")
message(STATUS "  BUILD_EXAMPLES: Build example programs (current: ${BUILD_EXAMPLES})")
message(STATUS "  BUILD_TESTS: Build unit tests (current: ${BUILD_TESTS})")
message(STATUS "  BUILD_APPS: Build RTSP applications (current: ${BUILD_APPS})")
message(STATUS "")
message(STATUS "Installation Options:")
message(STATUS "  CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  INSTALL_TO_USER_LOCAL: Install to ~/.local (current: ${INSTALL_TO_USER_LOCAL})")
message(STATUS "")
message(STATUS "Usage Examples:")
message(STATUS "  Debug build:     cmake -DCMAKE_BUILD_TYPE=Debug ..")
message(STATUS "  User install:    cmake -DINSTALL_TO_USER_LOCAL=ON ..")
message(STATUS "  Custom prefix:   cmake -DCMAKE_INSTALL_PREFIX=/opt/rtsp ..")
message(STATUS "  Minimal build:   cmake -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF ..")
message(STATUS "")
message(STATUS "============================================================")

# Handle user-local installation
if(INSTALL_TO_USER_LOCAL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "User local install path" FORCE)
    message(STATUS "Installing to user directory: ${CMAKE_INSTALL_PREFIX}")
endif()

# Provide install path information to users
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(STATUS "Using default install prefix: ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "You can customize it with: cmake -DCMAKE_INSTALL_PREFIX=/your/path ..")
endif()

# Enable position independent code for shared library compatibility
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Try to find installed coreutils first, fallback to local source if not found
find_package(coreutils QUIET)
if(coreutils_FOUND)
    message(STATUS "Using system-installed coreutils library")
    set(COREUTILS_SOURCE "system")
else()
    # Check if local coreutils directory exists
    set(COREUTILS_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../coreutils")
    if(EXISTS "${COREUTILS_LOCAL_DIR}/CMakeLists.txt")
        if(NOT TARGET coreutils)
            add_subdirectory(../coreutils coreutils_build)
        endif()
        message(STATUS "Using local coreutils from sibling directory")
        set(COREUTILS_SOURCE "local")
    else()
        # Neither system nor local coreutils found - show helpful error
        message(STATUS "")
        message(STATUS "========== DEPENDENCY ERROR ==========")
        message(FATAL_ERROR 
            "Cannot find coreutils dependency!\n"
            "\n"
            "Searched locations:\n"
            "  1. System installation (using find_package coreutils)\n"
            "  2. Local directory: ${COREUTILS_LOCAL_DIR}\n"
            "\n"
            "Solution - Download coreutils to sibling directory:\n"
            "\n"
            "    cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
            "    git clone ssh://git@github.com/lmshao/coreutils.git\n"
            "\n"
            "    # OR if SSH is not available:\n"
            "    git clone https://github.com/lmshao/coreutils.git\n"
            "\n"
            "Repository Information:\n"
            "  📍 GitHub: https://github.com/lmshao/coreutils\n"
            "  🔑 SSH:    ssh://git@github.com/lmshao/coreutils.git\n"
            "\n"
            "After downloading, re-run: cmake .."
        )
    endif()
endif()

# Try to find installed network library, fallback to local source if not found
find_package(network QUIET)
if(network_FOUND)
    message(STATUS "Using system-installed network library")
    set(NETWORK_SOURCE "system")
else()
    # Check if local network directory exists
    set(NETWORK_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../network")
    if(EXISTS "${NETWORK_LOCAL_DIR}/CMakeLists.txt")
        if(NOT TARGET network)
            add_subdirectory(../network network_build)
        endif()
        message(STATUS "Using local network from sibling directory")
        set(NETWORK_SOURCE "local")
    else()
        # Neither system nor local network found - show helpful error
        message(STATUS "")
        message(STATUS "========== DEPENDENCY ERROR ==========")
        message(FATAL_ERROR 
            "Cannot find network dependency!\n"
            "\n"
            "Searched locations:\n"
            "  1. System installation (using find_package network)\n"
            "  2. Local directory: ${NETWORK_LOCAL_DIR}\n"
            "\n"
            "Solution - Download network to sibling directory:\n"
            "\n"
            "    cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
            "    git clone ssh://git@github.com/lmshao/network.git\n"
            "\n"
            "    # OR if SSH is not available:\n"
            "    git clone https://github.com/lmshao/network.git\n"
            "\n"
            "Repository Information:\n"
            "  📍 GitHub: https://github.com/lmshao/network\n"
            "  🔑 SSH:    ssh://git@github.com/lmshao/network.git\n"
            "\n"
            "After downloading, re-run: cmake .."
        )
    endif()
endif()

# Source files and include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/rtsp)
include_directories(${PROJECT_SOURCE_DIR}/include/rtp)
include_directories(${PROJECT_SOURCE_DIR}/src)

# Collect RTSP source files
file(GLOB_RECURSE RTSP_SOURCES "src/rtsp/*.cpp")
file(GLOB_RECURSE RTP_SOURCES "src/rtp/*.cpp")
file(GLOB_RECURSE MPEGTS_SOURCES "src/mpegts/*.cpp")
set(SOURCES ${RTSP_SOURCES} ${RTP_SOURCES} ${MPEGTS_SOURCES})

# Validate build options
if(NOT BUILD_STATIC_LIBS AND NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR "At least one of BUILD_STATIC_LIBS or BUILD_SHARED_LIBS must be ON")
endif()

# Compiler flags
if(MSVC)
    # MSVC specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Od /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /DNDEBUG /DRELEASE")
else()
    # GCC/Clang flags for Linux/Unix
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error -Wno-format-truncation -Wno-unused-result")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DRELEASE")
endif()

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "C++ flags for ${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")

# Find threads (required for network operations)
find_package(Threads REQUIRED)

# Print source file summary
list(LENGTH RTSP_SOURCES RTSP_COUNT)
list(LENGTH RTP_SOURCES RTP_COUNT)
list(LENGTH MPEGTS_SOURCES MPEGTS_COUNT)
message(STATUS "Found ${RTSP_COUNT} RTSP source files")
message(STATUS "Found ${RTP_COUNT} RTP source files")
message(STATUS "Found ${MPEGTS_COUNT} MPEG-TS source files")

# Static library
if(BUILD_STATIC_LIBS)
    add_library(rtsp_static STATIC ${SOURCES})
    
    target_include_directories(rtsp_static PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    
    # Link dependencies - prefer static to static linking
    if(coreutils_FOUND)
        target_link_libraries(rtsp_static PUBLIC 
            $<IF:$<TARGET_EXISTS:coreutils::coreutils-static>,coreutils::coreutils-static,coreutils::coreutils>
        )
    else()
        target_link_libraries(rtsp_static PUBLIC coreutils-static)
    endif()
    
    if(network_FOUND)
        target_link_libraries(rtsp_static PUBLIC 
            $<IF:$<TARGET_EXISTS:network::network_static>,network::network_static,network::network>
        )
    else()
        target_link_libraries(rtsp_static PUBLIC network_static)
    endif()
    
    target_link_libraries(rtsp_static PUBLIC Threads::Threads)
    
    set_target_properties(rtsp_static PROPERTIES
        OUTPUT_NAME rtsp
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    
    # Create alias for internal use
    add_library(rtsp::rtsp_static ALIAS rtsp_static)
    message(STATUS "Building static library: librtsp.a")
endif()

# Shared library  
if(BUILD_SHARED_LIBS)
    add_library(rtsp_shared SHARED ${SOURCES})
    
    target_include_directories(rtsp_shared PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    
    # Link dependencies - prefer shared to shared linking
    if(coreutils_FOUND)
        target_link_libraries(rtsp_shared PUBLIC coreutils::coreutils-shared)
    else()
        target_link_libraries(rtsp_shared PUBLIC coreutils-shared)
    endif()
    
    if(network_FOUND)
        target_link_libraries(rtsp_shared PUBLIC 
            $<IF:$<TARGET_EXISTS:network::network_shared>,network::network_shared,network::network>
        )
    else()
        target_link_libraries(rtsp_shared PUBLIC network_shared)
    endif()
    
    target_link_libraries(rtsp_shared PUBLIC Threads::Threads)
    
    set_target_properties(rtsp_shared PROPERTIES
        OUTPUT_NAME rtsp
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    
    # Create alias for internal use
    add_library(rtsp::rtsp_shared ALIAS rtsp_shared)
    message(STATUS "Building shared library: librtsp.so")
endif()

# Create default alias for examples and tests
if(BUILD_STATIC_LIBS)
    add_library(rtsp ALIAS rtsp_static)
    set(DEFAULT_LIBRARY_TYPE "static")
else()
    add_library(rtsp ALIAS rtsp_shared)
    set(DEFAULT_LIBRARY_TYPE "shared")
endif()

message(STATUS "Examples and tests will use ${DEFAULT_LIBRARY_TYPE} library")

# Build examples
if(BUILD_EXAMPLES)
    message(STATUS "Building RTSP examples...")
    
    # Create examples directory in build folder
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
    
    # Find all example source files
    file(GLOB EXAMPLE_SOURCES "examples/*.cpp")
    
    # Create executables for each example
    foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
        # Get filename without extension
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
        
        # Create executable with original filename
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
        
        # Set include directories for examples
        target_include_directories(${EXAMPLE_NAME} PRIVATE 
            ${PROJECT_SOURCE_DIR}/include
        )
        
        # Link to the default library (static or shared)
        target_link_libraries(${EXAMPLE_NAME} PRIVATE rtsp)
        
        # Set output directory
        set_target_properties(${EXAMPLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
        )
        
        message(STATUS "RTSP Example target: ${EXAMPLE_NAME}")
    endforeach()
endif()

# Build applications
if(BUILD_APPS)
    message(STATUS "Building RTSP applications...")
    add_subdirectory(apps)
endif()

# Build tests
if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Building RTSP unit tests...")
    add_subdirectory(tests)
endif()

# Platform-specific installation settings
if(WIN32)
    # Disable install/uninstall targets on Windows platform
    message(STATUS "Install/uninstall targets are disabled on Windows platform")
    message(STATUS "Use the library directly from the build directory")
    set(ENABLE_INSTALL OFF)
else()
    set(ENABLE_INSTALL ON)
endif()

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Only install if enabled
if(ENABLE_INSTALL)
    # Install headers
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    # Install libraries
    if(BUILD_STATIC_LIBS)
        install(TARGETS rtsp_static
            EXPORT rtspTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    if(BUILD_SHARED_LIBS)
        install(TARGETS rtsp_shared
            EXPORT rtspTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    # Install CMake targets
    install(EXPORT rtspTargets
        FILE rtspTargets.cmake
        NAMESPACE rtsp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtsp
    )

# Create config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/rtspConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rtspConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtsp
)

# Create version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/rtspConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

    # Install config files
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/rtspConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/rtspConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtsp
    )

    # Create uninstall target
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        @ONLY
    )

    add_custom_target(uninstall_rtsp
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        COMMENT "Uninstalling RTSP library..."
    )
endif()

# Print build configuration summary
message(STATUS "")
message(STATUS "========== RTSP Library Build Configuration ==========")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Coreutils source: ${COREUTILS_SOURCE}")
message(STATUS "Network source: ${NETWORK_SOURCE}")
message(STATUS "Build static library: ${BUILD_STATIC_LIBS}")
message(STATUS "Build shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build applications: ${BUILD_APPS}")
if(ENABLE_INSTALL)
    message(STATUS "Install support: ON")
    message(STATUS "Uninstall support: ON (make uninstall_rtsp)")
else()
    message(STATUS "Install support: DISABLED (Windows platform)")
    message(STATUS "Uninstall support: DISABLED (Windows platform)")
endif()
message(STATUS "======================================================")
message(STATUS "")

