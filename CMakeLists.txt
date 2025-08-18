cmake_minimum_required(VERSION 3.20)
project(space_invaders LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
# Can't use glad building using python + jinja2 cause it needs venv to not break anything on linux
# So gotta do some tricks instead
FetchContent_Declare(
    BASIS
    GIT_REPOSITORY https://github.com/DisturbingSilence/BASIS
)
FetchContent_MakeAvailable(BASIS)

add_executable(demo src/main.cpp)

target_link_libraries(demo PUBLIC glad glfw)
target_compile_options(demo PRIVATE -Wall -pedantic -Wextra -O3)
