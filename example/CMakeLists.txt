cmake_minimum_required(VERSION 3.5)
project(example)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# list(APPEND CMAKE_PREFIX_PATH $ENV{DRAKE_BUILD_DIR})

find_package(GTest REQUIRED)

# INCLUDE($ENV{GITVERSION_CMAKE})
# TARGET_GIT_VERSION_INIT(ice_cream_scooper)

add_executable(main example/main.cpp)

# target_link_libraries(test_logging
#   ice_cream_scooper gflags
# )

# TARGET_GIT_VERSION_INIT(test_find_scoop)
