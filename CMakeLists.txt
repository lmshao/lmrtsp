cmake_minimum_required(VERSION 3.10)
project(lmrtsp VERSION 0.3.0 LANGUAGES CXX)

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

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Display configuration options for developers
message(STATUS "")
message(STATUS "========== lmrtsp Library Configuration Options ==========")
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
message(STATUS "  BUILD_APPS: Build lmrtsp applications (current: ${BUILD_APPS})")
message(STATUS "")
message(STATUS "Installation Options:")
message(STATUS "  CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  INSTALL_TO_USER_LOCAL: Install to ~/.local (current: ${INSTALL_TO_USER_LOCAL})")
message(STATUS "")
message(STATUS "Usage Examples:")
message(STATUS "  Debug build:     cmake -DCMAKE_BUILD_TYPE=Debug ..")
message(STATUS "  User install:    cmake -DINSTALL_TO_USER_LOCAL=ON ..")
message(STATUS "  Custom prefix:   cmake -DCMAKE_INSTALL_PREFIX=/opt/lmrtsp ..")
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

# Try to find installed lmcore first, fallback to local source if not found
find_package(lmcore QUIET)
if(lmcore_FOUND)
    message(STATUS "Using system-installed lmcore library")
    set(LMCORE_SOURCE "system")
else()
    # Check if local lmcore directory exists
    set(LMCORE_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lmcore")
    if(EXISTS "${LMCORE_LOCAL_DIR}/CMakeLists.txt")
        if(NOT TARGET lmcore)
            add_subdirectory(../lmcore lmcore_build)
        endif()
        message(STATUS "Using local lmcore from sibling directory")
        set(LMCORE_SOURCE "local")
    else()
        # Neither system nor local lmcore found - show helpful error
        message(STATUS "")
        message(STATUS "========== DEPENDENCY ERROR ==========")
        message(FATAL_ERROR
            "Cannot find lmcore dependency!\n"
            "\n"
            "Searched locations:\n"
            "  1. System installation (using find_package lmcore)\n"
            "  2. Local directory: ${LMCORE_LOCAL_DIR}\n"
            "\n"
            "Solution - Download lmcore to sibling directory:\n"
            "\n"
            "    cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
            "    git clone ssh://git@github.com/lmshao/lmcore.git\n"
            "\n"
            "    # OR if SSH is not available:\n"
            "    git clone https://github.com/lmshao/lmcore.git\n"
            "\n"
            "Repository Information:\n"
            "  📍 GitHub: https://github.com/lmshao/lmcore\n"
            "  🔑 SSH:    ssh://git@github.com/lmshao/lmcore.git\n"
            "\n"
            "After downloading, re-run: cmake .."
        )
    endif()
endif()

# Try to find installed lmnet library, fallback to local source if not found
find_package(lmnet QUIET)
if(lmnet_FOUND)
    message(STATUS "Using system-installed lmnet library")
    set(LMNET_SOURCE "system")
else()
    # Check if local lmnet directory exists
    set(LMNET_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lmnet")
    if(EXISTS "${LMNET_LOCAL_DIR}/CMakeLists.txt")
        if(NOT TARGET lmnet)
            add_subdirectory(../lmnet lmnet_build)
        endif()
        message(STATUS "Using local lmnet from sibling directory")
        set(LMNET_SOURCE "local")
    else()
        # Neither system nor local lmnet found - show helpful error
        message(STATUS "")
        message(STATUS "========== DEPENDENCY ERROR ==========")
        message(FATAL_ERROR
            "Cannot find lmnet dependency!\n"
            "\n"
            "Searched locations:\n"
            "  1. System installation (using find_package lmnet)\n"
            "  2. Local directory: ${LMNET_LOCAL_DIR}\n"
            "\n"
            "Solution - Download lmnet to sibling directory:\n"
            "\n"
            "    cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
            "    git clone ssh://git@github.com/lmshao/lmnet.git\n"
            "\n"
            "    # OR if SSH is not available:\n"
            "    git clone https://github.com/lmshao/lmnet.git\n"
            "\n"
            "Repository Information:\n"
            "  📍 GitHub: https://github.com/lmshao/lmnet\n"
            "  🔑 SSH:    ssh://git@github.com/lmshao/lmnet.git\n"
            "\n"
            "After downloading, re-run: cmake .."
        )
    endif()
endif()

# Source files and include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src)

# Collect all source files from src directory
file(GLOB_RECURSE SOURCES "src/*.cpp")

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

# Find threads (required for lmnet operations)
find_package(Threads REQUIRED)

# Print source file summary
list(LENGTH SOURCES SOURCE_COUNT)
message(STATUS "Found ${SOURCE_COUNT} source files in src/")

# Static library
if(BUILD_STATIC_LIBS)
    add_library(lmrtsp_static STATIC ${SOURCES})

    target_include_directories(lmrtsp_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Link dependencies - prefer static to static linking
    if(lmcore_FOUND)
        target_link_libraries(lmrtsp_static PUBLIC
            $<IF:$<TARGET_EXISTS:lmcore::lmcore-static>,lmcore::lmcore-static,lmcore::lmcore>
        )
    else()
        target_link_libraries(lmrtsp_static PUBLIC lmcore-static)
    endif()

    if(lmnet_FOUND)
        target_link_libraries(lmrtsp_static PUBLIC
            $<IF:$<TARGET_EXISTS:lmnet::lmnet_static>,lmnet::lmnet_static,lmnet::lmnet>
        )
    else()
        target_link_libraries(lmrtsp_static PUBLIC lmnet_static)
    endif()

    target_link_libraries(lmrtsp_static PUBLIC Threads::Threads)

    set_target_properties(lmrtsp_static PROPERTIES
        OUTPUT_NAME lmrtsp
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    # Create alias for internal use
    add_library(lmrtsp::lmrtsp_static ALIAS lmrtsp_static)
    message(STATUS "Building static library: liblmrtsp.a")
endif()

# Shared library  
if(BUILD_SHARED_LIBS)
    add_library(lmrtsp_shared SHARED ${SOURCES})

    target_include_directories(lmrtsp_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Link dependencies - prefer shared to shared linking
    if(lmcore_FOUND)
        target_link_libraries(lmrtsp_shared PUBLIC lmcore::lmcore-shared)
    else()
        target_link_libraries(lmrtsp_shared PUBLIC lmcore-shared)
    endif()

    if(lmnet_FOUND)
        target_link_libraries(lmrtsp_shared PUBLIC
            $<IF:$<TARGET_EXISTS:lmnet::lmnet_shared>,lmnet::lmnet_shared,lmnet::lmnet>
        )
    else()
        target_link_libraries(lmrtsp_shared PUBLIC lmnet_shared)
    endif()

    target_link_libraries(lmrtsp_shared PUBLIC Threads::Threads)

    set_target_properties(lmrtsp_shared PROPERTIES
        OUTPUT_NAME lmrtsp
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    # Create alias for internal use
    add_library(lmrtsp::lmrtsp_shared ALIAS lmrtsp_shared)
    message(STATUS "Building shared library: liblmrtsp.so")
endif()

# Create default alias for examples and tests
if(BUILD_STATIC_LIBS)
    add_library(lmrtsp ALIAS lmrtsp_static)
    set(DEFAULT_LIBRARY_TYPE "static")
else()
    add_library(lmrtsp ALIAS lmrtsp_shared)
    set(DEFAULT_LIBRARY_TYPE "shared")
endif()

message(STATUS "Examples and tests will use ${DEFAULT_LIBRARY_TYPE} library")

# Build examples
if(BUILD_EXAMPLES)
    message(STATUS "Building lmrtsp examples...")

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
        target_link_libraries(${EXAMPLE_NAME} PRIVATE lmrtsp)

        # Set output directory
        set_target_properties(${EXAMPLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
        )

        message(STATUS "lmrtsp Example target: ${EXAMPLE_NAME}")
    endforeach()
endif()

# Build applications
if(BUILD_APPS)
    message(STATUS "Building lmrtsp applications...")
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
        install(TARGETS lmrtsp_static
            EXPORT lmrtspTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    if(BUILD_SHARED_LIBS)
        install(TARGETS lmrtsp_shared
            EXPORT lmrtspTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    # Install CMake targets
    install(EXPORT lmrtspTargets
        FILE lmrtspTargets.cmake
        NAMESPACE lmrtsp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lmrtsp
    )

    # Create config file
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lmrtspConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/lmrtspConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lmrtsp
    )

    # Create version file
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/lmrtspConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/lmrtspConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/lmrtspConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lmrtsp
    )

    # Create uninstall target
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        @ONLY
    )

    # Provide a convenient generic 'uninstall' target name for users
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        COMMENT "Uninstalling lmrtsp library... (generic 'uninstall' target)"
    )
endif()

# Print build configuration summary
message(STATUS "")
message(STATUS "========== lmrtsp Library Build Configuration ==========")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Lmcore source: ${LMCORE_SOURCE}")
message(STATUS "Lmnet source: ${LMNET_SOURCE}")
message(STATUS "Build static library: ${BUILD_STATIC_LIBS}")
message(STATUS "Build shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build applications: ${BUILD_APPS}")
if(ENABLE_INSTALL)
    message(STATUS "Install support: ON")
    message(STATUS "Uninstall support: ON (make uninstall)")
else()
    message(STATUS "Install support: DISABLED (Windows platform)")
    message(STATUS "Uninstall support: DISABLED (Windows platform)")
endif()
message(STATUS "======================================================")
message(STATUS "")

