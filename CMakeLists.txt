cmake_minimum_required(VERSION 3.2.2)

project(seq)

set(CMAKE_CXX_STANDARD 11)


include_directories(./)


set(CMAKE_CXX_FLAGS_DEBUG "-pipe -DDEBUG -Wall -gdwarf-3 -gstrict-dwarf -O0 ${CMAKE_CXX_FLAGS_ALL}")

set(CMAKE_CXX_FLAGS_RELEASE "-pipe -DNDEBUG -Wall -g0 -O2 ${CMAKE_CXX_FLAGS_ALL}")


set(SOURCE_FILES Sequence.cpp TcpServer.cpp main.cpp)

add_executable(seq ${SOURCE_FILES})

target_link_libraries(seq pthread)
