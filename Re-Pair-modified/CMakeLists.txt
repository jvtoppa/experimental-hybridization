cmake_minimum_required(VERSION 3.1) # Bumped version for modern C++ standards, though 2.6 works

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

project (repair)

include_directories(${PROJECT_SOURCE_DIR}/internal)
include_directories(${PROJECT_SOURCE_DIR})
# Include the malloc_count directory so its header files can be found easily
include_directories(${PROJECT_SOURCE_DIR}/internal/malloc_count-0.7.1)

message("Building in ${CMAKE_BUILD_TYPE} mode")

# A cleaner way to set standard in modern CMake, but keeping your flags intact:
set(CMAKE_CXX_FLAGS "--std=c++23")

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb -g -p")
set(CMAKE_CXX_FLAGS_RELEASE "-ggdb -Ofast -fstrict-aliasing -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -ggdb -Ofast -fstrict-aliasing -march=native")

# Add the extra source files here (including the C file)
add_executable(rp 
    rp.cpp 
    internal/bitvector.cpp 
    internal/gcis/gcis.cpp
    internal/malloc_count-0.7.1/malloc_count.c
)

# malloc_count requires the system's dl library to hook memory functions on Linux/macOS
target_link_libraries(rp ${CMAKE_DL_LIBS})