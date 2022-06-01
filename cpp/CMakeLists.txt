cmake_minimum_required(VERSION 3.1)

project(BomberRobots)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# -Werror
set(CMAKE_CXX_FLAGS "-std=gnu++20 -Wall -Wextra -Wconversion -Werror -O2")

find_package(Boost 1.74.0 COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIR} include)

add_executable(robots-client client.cpp)
target_link_libraries(robots-client ${Boost_LIBRARIES})