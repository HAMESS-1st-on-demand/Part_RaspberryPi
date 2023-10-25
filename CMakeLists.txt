cmake_minimum_required(VERSION 3.0.0)
project(Proj1 VERSION 0.1.0 LANGUAGES C)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# add_executable(Proj1 Serial_test1.c)
# add_executable(Proj1 ./serial_with_raspberry/looptest.c)
# add_executable(Proj1 ./serial_with_raspberry/termioreceive.c)
# add_executable(Proj1 termiouart3.c)
add_executable(Proj1 ./integration/test1.c)
target_link_libraries(Proj1 wiringPi)
target_link_libraries(Proj1 bcm2835)
target_link_libraries(Proj1 pthread)
