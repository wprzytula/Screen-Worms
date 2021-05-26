cmake_minimum_required(VERSION 3.12)
project(Robaki)

set(CMAKE_CXX_STANDARD 17)

#set(CMAKE_CXX_COMPILER clang)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -std=gnu++17")

add_library(err Common/err.cpp Common/err.h)

add_executable(screen-worms-client client_main.cpp Client/gai_sock_factory.cpp Common/Event.h Common/Buffer.h Common/Crc32Computer.h Common/ClientHeartbeat.h Server/Pixel.h Common/Epoll.h Client/Client.h Common/Buffer.cpp Client/Client.cpp)
target_link_libraries(screen-worms-client err)
add_executable(screen-worms-server server_main.cpp Common/Event.h Common/Buffer.h Server/RandomGenerator.h Server/Board.h Server/GameConstants.h Common/Crc32Computer.h Common/ClientHeartbeat.h Server/Pixel.h Server/ClientData.h Common/Epoll.h Server/Player.h Server/Game.h Server/Server.h Common/Buffer.cpp Server/Server.cpp Server/Game.cpp)
target_link_libraries(screen-worms-server err)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)

add_executable(GUI GUI/net.c GUI/read_line.c GUI/gui2.c GUI/err.c GUI/cmd.c)

include_directories (${GTK2_INCLUDE_DIRS})
link_directories (${GTK2_LIBRARY_DIRS})
add_definitions (${GTK2_CFLAGS_OTHER})

target_link_libraries(GUI PRIVATE ${GTK2_LIBRARIES})

target_compile_definitions(GUI PRIVATE ${GTKMM_CFLAGS_OTHER})