cmake_minimum_required(VERSION 3.25)

project(HIDMouse VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "HID-Mouse v1 requires Windows and MSVC")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "HID-Mouse v1 supports x64 builds only")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HID_MOUSE_BUILD_TESTS "Build unit tests" ON)
option(HID_MOUSE_BUILD_WINUHID "Build WinUHid user-mode DLLs" ON)

set(HID_MOUSE_SOURCES
  src/app_types.cpp
  src/cli.cpp
  src/event_logger.cpp
  src/input_bridge.cpp
  src/platform.cpp
  src/source_classifier.cpp
  src/winuhid_adapter.cpp
)

add_executable(hid-mouse
  src/main.cpp
  ${HID_MOUSE_SOURCES}
)

target_include_directories(hid-mouse PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/WinUHid/WinUHid
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/WinUHid/WinUHidDevs
)

target_compile_definitions(hid-mouse PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

target_compile_options(hid-mouse PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_link_options(hid-mouse PRIVATE "/MANIFESTUAC:level='asInvoker' uiAccess='false'")
target_link_libraries(hid-mouse PRIVATE user32 advapi32)

if(HID_MOUSE_BUILD_WINUHID)
  include(cmake/WinUHid.cmake)
  hid_mouse_add_winuhid(hid-mouse)
endif()

include(CTest)
if(BUILD_TESTING AND HID_MOUSE_BUILD_TESTS)
  add_executable(hid-mouse-tests
    tests/test_main.cpp
    src/app_types.cpp
    src/cli.cpp
    src/event_logger.cpp
    src/source_classifier.cpp
  )
  target_include_directories(hid-mouse-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(hid-mouse-tests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  target_compile_options(hid-mouse-tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
  target_link_libraries(hid-mouse-tests PRIVATE user32)

  add_test(NAME unit COMMAND hid-mouse-tests)
  add_test(NAME cli-help COMMAND hid-mouse --help)
endif()
