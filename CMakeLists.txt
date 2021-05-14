cmake_minimum_required(VERSION 3.19)
project(Robaki)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -std=c++17")

add_library(err err.c)

add_executable(screen-worms-client client.cpp event.cpp event.h)
target_link_libraries(screen-worms-client err)
add_executable(screen-worms-server server.cpp event.cpp event.h)
target_link_libraries(screen-worms-server err)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)

add_executable(GUI GUI/net.c GUI/read_line.c GUI/gui2.c GUI/err.c GUI/cmd.c)

include_directories (${GTK2_INCLUDE_DIRS})
link_directories (${GTK2_LIBRARY_DIRS})
add_definitions (${GTK2_CFLAGS_OTHER})

target_link_libraries(GUI PRIVATE ${GTK2_LIBRARIES})

target_compile_definitions(GUI PRIVATE ${GTKMM_CFLAGS_OTHER})