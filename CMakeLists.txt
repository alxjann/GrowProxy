cmake_minimum_required(VERSION 3.20)
project(Proxy CXX C)

if(NOT WIN32)
    message(FATAL_ERROR "Proxy only supports Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Build the C++ runtime into the exe, so it runs without the Visual C++ Redistributable.
# The policy default makes libraries with an older cmake_minimum_required (mbedtls) follow it too.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE) # GLFW's own runtime switch

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

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${LIB_ROOT}/glfw)

add_library(imgui STATIC
    ${LIB_ROOT}/imgui/imgui.cpp
    ${LIB_ROOT}/imgui/imgui_demo.cpp
    ${LIB_ROOT}/imgui/imgui_draw.cpp
    ${LIB_ROOT}/imgui/imgui_tables.cpp
    ${LIB_ROOT}/imgui/imgui_widgets.cpp
    ${LIB_ROOT}/imgui/imgui_impl_glfw.cpp
    ${LIB_ROOT}/imgui/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${LIB_ROOT}/imgui)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(imgui PUBLIC glfw opengl32)

# --- proxy ------------------------------------------------------------------

# Everything except the entry point and the UI, shared by the proxy and the tests.
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

# Dear ImGui front end (GLFW + OpenGL 3). The executable is a windowed app; --headless runs it in a console.
file(GLOB_RECURSE PROXY_UI_SOURCES CONFIGURE_DEPENDS ${SOURCE_ROOT}/UI/*.cpp)

add_executable(Proxy WIN32 ${SOURCE_ROOT}/Main.cpp ${PROXY_UI_SOURCES})
target_link_libraries(Proxy PRIVATE ProxyCore imgui dwmapi)
if(MSVC)
    # keep main() as the entry point of the WIN32 subsystem executable; /bigobj for the embedded fonts
    target_link_options(Proxy PRIVATE /ENTRY:mainCRTStartup)
    target_compile_options(Proxy PRIVATE /bigobj /utf-8)
endif()
set_target_properties(Proxy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Proxy"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Proxy"
)

# Tests are local only (Tests/ is git-ignored); build them when the folder is present.
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/Tests/ParserTests.cpp)
    # Parser regression check against dumps from known-good data (see Tests/ParserTests.cpp).
    add_executable(ParserTests ${CMAKE_CURRENT_LIST_DIR}/Tests/ParserTests.cpp)
    target_link_libraries(ParserTests PRIVATE ProxyCore)
endif()

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/Tests/ProxyTests.cpp)
    # Unit checks and an end-to-end proxy test over localhost ENet (see Tests/ProxyTests.cpp).
    add_executable(ProxyTests ${CMAKE_CURRENT_LIST_DIR}/Tests/ProxyTests.cpp)
    target_link_libraries(ProxyTests PRIVATE ProxyCore)
endif()
