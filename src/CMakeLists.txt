cmake_minimum_required(VERSION 2.8.12)

set(CMAKE_C_FLAGS "-Wall")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_INSTALL_PREFIX}/share/buzz/cmake)

find_package(Buzz REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/buzz ${BUZZ_C_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/buzz ${KHEPERA_INCLUDE_DIR} ${BUZZ_C_INCLUDE_DIR})
link_directories(${KHEPERA_LIBRARY_DIR})

add_executable(bzzkh4
  buzz/buzz_utility.c buzz/buzz_utility.h
  buzz/buzzkh4_closures.c buzz/buzzkh4_closures.h
  buzz/kh4_utility.c buzz/kh4_utility.h
  buzz/kh4_camera.c buzz/kh4_camera.h
  buzz/bzzkh4.c)
target_link_libraries(bzzkh4 ${BUZZ_LIBRARY} ${BUZZ_LIBRARY_DEBUG} -lkhepera -lpthread)

install(TARGETS bzzkh4 RUNTIME DESTINATION bin)
