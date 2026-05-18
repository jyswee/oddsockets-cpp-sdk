cmake_minimum_required(VERSION 3.12)

# Project configuration
project(OddSocketsCppSDK
    VERSION 1.0.0
    DESCRIPTION "OddSockets C++ SDK for embedded systems and IoT devices"
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(ODDSOCKETS_BUILD_EXAMPLES "Build example applications" ON)
option(ODDSOCKETS_BUILD_TESTS "Build unit tests" ON)
option(ODDSOCKETS_HEADER_ONLY "Build as header-only library" OFF)
option(ODDSOCKETS_ENABLE_SSL "Enable SSL/TLS support" ON)
option(ODDSOCKETS_ENABLE_LOGGING "Enable logging support" ON)
option(ODDSOCKETS_ENABLE_THREADING "Enable threading support" ON)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_WIN32_WINNT=0x0601)  # Windows 7+
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

# Find dependencies
find_package(Threads REQUIRED)

# Optional SSL support
if(ODDSOCKETS_ENABLE_SSL)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        add_compile_definitions(ODDSOCKETS_ENABLE_SSL)
    else()
        message(WARNING "OpenSSL not found, SSL support disabled")
        set(ODDSOCKETS_ENABLE_SSL OFF)
    endif()
endif()

# Optional WebSocket library (prefer libwebsockets, fallback to websocketpp)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBWEBSOCKETS libwebsockets)
endif()

if(NOT LIBWEBSOCKETS_FOUND)
    find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/config/asio_no_tls.hpp)
    if(WEBSOCKETPP_INCLUDE_DIR)
        set(WEBSOCKETPP_FOUND TRUE)
        add_compile_definitions(ODDSOCKETS_USE_WEBSOCKETPP)
    endif()
endif()

# Create main library target
if(ODDSOCKETS_HEADER_ONLY)
    add_library(oddsockets INTERFACE)
    target_include_directories(oddsockets INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    add_compile_definitions(ODDSOCKETS_HEADER_ONLY)
else()
    # Source files (when not header-only)
    set(ODDSOCKETS_SOURCES
        src/Types.cpp
        src/OddSockets.cpp
        src/Channel.cpp
        src/ManagerDiscovery.cpp
        src/MessageSizeValidator.cpp
        src/WebSocketClient.cpp
        src/HttpClient.cpp
        src/JsonParser.cpp
    )
    
    add_library(oddsockets ${ODDSOCKETS_SOURCES})
    target_include_directories(oddsockets PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endif()

# Link dependencies
target_link_libraries(oddsockets 
    $<$<NOT:$<BOOL:${ODDSOCKETS_HEADER_ONLY}>>:Threads::Threads>
)

if(ODDSOCKETS_ENABLE_SSL AND OpenSSL_FOUND)
    target_link_libraries(oddsockets 
        $<$<NOT:$<BOOL:${ODDSOCKETS_HEADER_ONLY}>>:OpenSSL::SSL OpenSSL::Crypto>
    )
endif()

if(LIBWEBSOCKETS_FOUND)
    target_include_directories(oddsockets 
        $<$<NOT:$<BOOL:${ODDSOCKETS_HEADER_ONLY}>>:PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS}>
    )
    target_link_libraries(oddsockets 
        $<$<NOT:$<BOOL:${ODDSOCKETS_HEADER_ONLY}>>:${LIBWEBSOCKETS_LIBRARIES}>
    )
    add_compile_definitions(ODDSOCKETS_USE_LIBWEBSOCKETS)
elseif(WEBSOCKETPP_FOUND)
    target_include_directories(oddsockets 
        $<$<NOT:$<BOOL:${ODDSOCKETS_HEADER_ONLY}>>:PRIVATE ${WEBSOCKETPP_INCLUDE_DIR}>
    )
endif()

# Compiler definitions
if(ODDSOCKETS_ENABLE_LOGGING)
    add_compile_definitions(ODDSOCKETS_ENABLE_LOGGING)
endif()

if(ODDSOCKETS_ENABLE_THREADING)
    add_compile_definitions(ODDSOCKETS_ENABLE_THREADING)
endif()

# Embedded system optimizations
if(CMAKE_SYSTEM_NAME MATCHES "Generic" OR DEFINED EMBEDDED_BUILD)
    add_compile_definitions(ODDSOCKETS_EMBEDDED_BUILD)
    # Disable features that may not be available on embedded systems
    set(ODDSOCKETS_ENABLE_SSL OFF)
    set(ODDSOCKETS_ENABLE_THREADING OFF)
    add_compile_definitions(ODDSOCKETS_NO_EXCEPTIONS)
    add_compile_options(-fno-exceptions -fno-rtti)
endif()

# Create alias for easier usage
add_library(OddSockets::oddsockets ALIAS oddsockets)

# Examples
if(ODDSOCKETS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(ODDSOCKETS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Install library (if not header-only)
if(NOT ODDSOCKETS_HEADER_ONLY)
    install(TARGETS oddsockets
        EXPORT OddSocketsTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install CMake config files
install(EXPORT OddSocketsTargets
    FILE OddSocketsTargets.cmake
    NAMESPACE OddSockets::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OddSockets
)

# Create config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/OddSocketsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OddSocketsConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OddSockets
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OddSocketsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/OddSocketsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OddSocketsConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OddSockets
)

# Package configuration
set(CPACK_PACKAGE_NAME "OddSocketsCppSDK")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "OddSockets")
set(CPACK_PACKAGE_CONTACT "support@oddsockets.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Platform-specific packaging
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl-dev, libwebsockets-dev")
    set(CPACK_RPM_PACKAGE_REQUIRES "openssl-devel, libwebsockets-devel")
endif()

include(CPack)

# Print configuration summary
message(STATUS "OddSockets C++ SDK Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Header Only: ${ODDSOCKETS_HEADER_ONLY}")
message(STATUS "  SSL Support: ${ODDSOCKETS_ENABLE_SSL}")
message(STATUS "  Logging: ${ODDSOCKETS_ENABLE_LOGGING}")
message(STATUS "  Threading: ${ODDSOCKETS_ENABLE_THREADING}")
message(STATUS "  Build Examples: ${ODDSOCKETS_BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${ODDSOCKETS_BUILD_TESTS}")

if(LIBWEBSOCKETS_FOUND)
    message(STATUS "  WebSocket Library: libwebsockets")
elseif(WEBSOCKETPP_FOUND)
    message(STATUS "  WebSocket Library: websocketpp")
else()
    message(STATUS "  WebSocket Library: Built-in (limited functionality)")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Generic" OR DEFINED EMBEDDED_BUILD)
    message(STATUS "  Target: Embedded System")
else()
    message(STATUS "  Target: Desktop/Server")
endif()
