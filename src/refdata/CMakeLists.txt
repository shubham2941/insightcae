project(refdata)
#find_package(Boost COMPONENTS filesystem regex date_time thread REQUIRED)

set(refdata_SOURCES refdata.cpp)


link_directories(${Boost_LIBRARY_DIR})

add_library(refdata SHARED ${refdata_SOURCES})
target_include_directories(refdata 
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR} 
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  )
target_link_libraries(refdata toolkit)

install(TARGETS refdata LIBRARY DESTINATION lib)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
  DESTINATION include/insightcae
  FILES_MATCHING PATTERN "*.h"
  )

