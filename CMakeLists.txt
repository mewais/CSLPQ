cmake_minimum_required(VERSION 3.10)
project(SkipListPriorityQueue)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE  "Release")

##################################################################################
################################### Library ######################################
##################################################################################
file(GLOB header_files include/SkipListPriorityQueue/*.hpp)

add_library(SkipListPriorityQueue INTERFACE)
target_include_directories(SkipListPriorityQueue INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include
        DESTINATION ${CMAKE_INSTALL_PREFIX})

###################################################################################
##################################### test #######################################
###################################################################################
include(CTest)
enable_testing()
file(GLOB tests test/*.cpp)
include_directories(include/)

foreach(test ${tests})
    string(REGEX REPLACE "(^.*/|\\.[^.]*$)" "" basetest ${test})
    add_executable(${basetest} ${test})
    add_test(${basetest} ${basetest})
endforeach()