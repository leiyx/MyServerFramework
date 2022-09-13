cmake_minimum_required(VERSION 3.0)
project(serverframework)

include (utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE OFF)

# 指定编译选项
# set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++17 -O0 -ggdb -Wall -Werror")
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++11 -O0 -ggdb -Wall")

# -rdynamic: 将所有符号都加入到符号表中，便于使用dlopen或者backtrace追踪到符号
# -fPIC: 生成位置无关的代码，便于动态链接
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -fPIC")

# -Wno-unused-function: 不要警告未使用函数
# -Wno-builtin-macro-redefined: 不要警告内置宏重定义，用于重定义内置的__FILE__宏
# -Wno-deprecated: 不要警告过时的特性
# -Wno-deprecated-declarations: 不要警告使用带deprecated属性的变量，类型，函数
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated -Wno-deprecated-declarations")

include_directories(.)

option(BUILD_TEST "ON for complile test" ON)

find_package(Boost REQUIRED) 
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

set(LIB_SRC
    serverframework/log.cpp
    serverframework/util.cpp
    serverframework/mutex.cc
    serverframework/env.cc
    serverframework/config.cc
    serverframework/thread.cc
    serverframework/fiber.cc
    serverframework/scheduler.cc
    serverframework/iomanager.cc
    serverframework/timer.cc
    serverframework/fd_manager.cc
    serverframework/hook.cc
    serverframework/address.cc 
    serverframework/socket.cc 
    serverframework/bytearray.cc 
    serverframework/tcp_server.cc 
    serverframework/daemon.cc 
    )

add_library(serverframework SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(serverframework)

set(LIBS
    serverframework
    pthread
    dl
    yaml-cpp
)

if(BUILD_TEST)
my_add_executable(test_log "tests/test_log.cpp" serverframework "${LIBS}")
my_add_executable(test_util "tests/test_util.cpp" serverframework "${LIBS}")
my_add_executable(test_env "tests/test_env.cc" serverframework "${LIBS}")
my_add_executable(test_config "tests/test_config.cc" serverframework "${LIBS}")
my_add_executable(test_thread "tests/test_thread.cc" serverframework "${LIBS}")
my_add_executable(test_fiber1 "tests/test_fiber1.cc" serverframework "${LIBS}")
my_add_executable(test_fiber2 "tests/test_fiber2.cc" serverframework "${LIBS}")
my_add_executable(test_scheduler "tests/test_scheduler.cc" serverframework "${LIBS}")
my_add_executable(test_iomanager "tests/test_iomanager.cc" serverframework "${LIBS}")
my_add_executable(test_timer "tests/test_timer.cc" serverframework "${LIBS}")
my_add_executable(test_hook "tests/test_hook.cc" serverframework "${LIBS}")
my_add_executable(test_address "tests/test_address.cc" serverframework "${LIBS}")
my_add_executable(test_socket_tcp_server "tests/test_socket_tcp_server.cc" serverframework "${LIBS}")
my_add_executable(test_socket_tcp_client "tests/test_socket_tcp_client.cc" serverframework "${LIBS}")
my_add_executable(test_bytearray "tests/test_bytearray.cc" serverframework "${LIBS}")
my_add_executable(test_tcp_server "tests/test_tcp_server.cc" serverframework "${LIBS}")
my_add_executable(test_daemon "tests/test_daemon.cc" serverframework "${LIBS}")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)