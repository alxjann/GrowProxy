cmake_minimum_required(VERSION 3.20)
project(Proxy CXX C)

if(NOT WIN32)
    message(FATAL_ERROR "Proxy only supports Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/Source")
set(LIB_ROOT "${CMAKE_CURRENT_LIST_DIR}/Lib")

# --- third party ------------------------------------------------------------

add_library(enet STATIC
    ${LIB_ROOT}/enet/callbacks.c
    ${LIB_ROOT}/enet/compress.c
    ${LIB_ROOT}/enet/host.c
    ${LIB_ROOT}/enet/list.c
    ${LIB_ROOT}/enet/packet.c
    ${LIB_ROOT}/enet/peer.c
    ${LIB_ROOT}/enet/protocol.c
    ${LIB_ROOT}/enet/win32.c
)
target_include_directories(enet PUBLIC ${LIB_ROOT}/enet/include)
target_link_libraries(enet PUBLIC ws2_32 winmm)

add_library(zlib STATIC
    ${LIB_ROOT}/zlib/adler32.c
    ${LIB_ROOT}/zlib/crc32.c
    ${LIB_ROOT}/zlib/inffast.c
    ${LIB_ROOT}/zlib/inflate.c
    ${LIB_ROOT}/zlib/inftrees.c
    ${LIB_ROOT}/zlib/uncompr.c
    ${LIB_ROOT}/zlib/zutil.c
)
target_include_directories(zlib PUBLIC ${LIB_ROOT}/zlib)

set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
add_subdirectory(${LIB_ROOT}/mbedtls)

# --- proxy ------------------------------------------------------------------

# Everything except the entry point, shared by the proxy and the tests.
# Source/UI is not built: the proxy currently runs as a console application.
file(GLOB_RECURSE PROXY_CORE_SOURCES CONFIGURE_DEPENDS
    ${SOURCE_ROOT}/Core/*.cpp
    ${SOURCE_ROOT}/Packets/*.cpp
    ${SOURCE_ROOT}/Utils/*.cpp
    ${SOURCE_ROOT}/Game/*.cpp
    ${SOURCE_ROOT}/Network/*.cpp
    ${SOURCE_ROOT}/Features/*.cpp
    ${SOURCE_ROOT}/Proxy/*.cpp
)

add_library(ProxyCore STATIC ${PROXY_CORE_SOURCES})
target_include_directories(ProxyCore PUBLIC ${SOURCE_ROOT})
target_compile_definitions(ProxyCore PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ProxyCore PUBLIC enet zlib mbedtls mbedx509 mbedcrypto winhttp)

add_executable(Proxy ${SOURCE_ROOT}/Main.cpp)
target_link_libraries(Proxy PRIVATE ProxyCore)
set_target_properties(Proxy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Bin"
)

# Parser regression check against dumps from known-good data (see Tests/ParserTests.cpp).
add_executable(ParserTests ${CMAKE_CURRENT_LIST_DIR}/Tests/ParserTests.cpp)
target_link_libraries(ParserTests PRIVATE ProxyCore)

# Unit checks and an end-to-end proxy test over localhost ENet (see Tests/ProxyTests.cpp).
add_executable(ProxyTests ${CMAKE_CURRENT_LIST_DIR}/Tests/ProxyTests.cpp)
target_link_libraries(ProxyTests PRIVATE ProxyCore)
