cmake_minimum_required(VERSION 3.8)
project(wj_716_lidar)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(wj_716_lidar_node
  src/wj_716_lidar_node.cpp
)

target_include_directories(wj_716_lidar_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

ament_target_dependencies(
  wj_716_lidar_node
  rclcpp
  sensor_msgs
)

install(TARGETS wj_716_lidar_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
