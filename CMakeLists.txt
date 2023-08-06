cmake_minimum_required(VERSION 3.10)
project(CSLPQ)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS "-Wall -Werror -Wno-narrowing -Wno-nonnull -pthread -O3")

option(ENABLE_TESTS "Enable tests" OFF)

##################################################################################
################################### Library ######################################
##################################################################################
file(GLOB header_files include/CSLPQ/*.hpp)

add_library(CSLPQ INTERFACE)
target_include_directories(CSLPQ INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include
        DESTINATION ${CMAKE_INSTALL_PREFIX})

###################################################################################
##################################### test #######################################
###################################################################################
if (${ENABLE_TESTS})
    include(CTest)
    enable_testing()
    file(GLOB tests test/*.cpp)
    include_directories(include/)

    foreach(test ${tests})
        string(REGEX REPLACE "(^.*/|\\.[^.]*$)" "" basetest ${test})
        add_executable(${basetest} ${test})
        add_test(${basetest} ${basetest})
    endforeach()
endif()