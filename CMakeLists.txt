cmake_minimum_required(VERSION 3.20)
project(Physarum LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(ndc
	GIT_REPOSITORY https://github.com/Bannerjee/ndc
 	GIT_TAG 31670da740782bba52bbf0bdcf5760947b741331)
FetchContent_MakeAvailable(ndc)

add_executable(demo src/main.cpp)
target_link_libraries(demo PRIVATE NDC)
target_compile_options(demo PRIVATE -Wall -pedantic -Wextra -O3)
