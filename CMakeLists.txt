cmake_minimum_required(VERSION 3.26)
project(Revelio)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/base.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/archive)

include(FetchContent)

# Dependencies

# Boost
set(BOOST_INCLUDE_LIBRARIES asio)
FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
    URL_MD5 893b5203b862eb9bbd08553e24ff146a
)
FetchContent_MakeAvailable(Boost)

add_subdirectory(libs/emp-tool)
add_subdirectory(libs/emp-ot)
add_subdirectory(libs/Kyber)
add_subdirectory(libs/emp-ag2pc)


# Target
set(LIB
    emp-tool
    emp-ot
    emp-ot-original
    emp-ag2pc
    Kyber
    Boost::asio
)
set(SRC
    src/socket.cpp
    src/PRNG.cpp
    src/utils.cpp
    src/EndemicOT/Endemic_OT_C.c
    src/EndemicOT/EndemicOT.cpp
    src/EndemicOT/OTTools.c
    include/global_key_sampling.hpp
)
set(HEADER
    include/socket.hpp
    include/PRNG.hpp
    include/utils.hpp
    include/EndemicOT/Endemic_OT_C.h
    include/EndemicOT/EndemicOT.hpp
    include/EndemicOT/OTTools.h
    include/block_correlated_OT.hpp
)

add_library(${PROJECT_NAME} STATIC ${SRC} ${HEADER})
target_include_directories(${PROJECT_NAME} PUBLIC include/)
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIB})


if (ENABLE_TEST)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    FetchContent_MakeAvailable(googletest)

    set(TEST_NAME ${PROJECT_NAME}-test)
    set(TEST_SRC
        tests/PRNG.test.cpp
        tests/io.test.cpp
        tests/EndemicOT.test.cpp
        tests/PRNG.test.cpp
        tests/IKNP.test.cpp
        tests/bCOT.test.cpp
        tests/global_key_sampling.test.cpp
    )

    add_executable(${TEST_NAME} ${TEST_SRC})
    target_include_directories(${TEST_NAME} PUBLIC include)
    target_link_libraries(
        ${TEST_NAME}
        ${PROJECT_NAME}
        gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(${TEST_NAME})

endif (ENABLE_TEST)
