# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(MatterElectricalMeterSolution)

include(ExternalProject)

ExternalProject_Add(MatterElectricalMeterOverThread
  SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../MatterElectricalMeterOverThread/cmake_gcc
  BINARY_DIR ${CMAKE_CURRENT_LIST_DIR}/../MatterElectricalMeterOverThread/cmake_gcc/build
  CMAKE_ARGS --preset project
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
  INSTALL_COMMAND ""
  BUILD_ALWAYS true
  USES_TERMINAL_BUILD 1
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../MatterElectricalMeterOverThread/cmake_gcc
)
ExternalProject_Add(Matter-Bootloader
  SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../Matter-Bootloader/cmake_gcc
  BINARY_DIR ${CMAKE_CURRENT_LIST_DIR}/../Matter-Bootloader/cmake_gcc/build
  CMAKE_ARGS --preset project
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
  INSTALL_COMMAND ""
  BUILD_ALWAYS true
  USES_TERMINAL_BUILD 1
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../Matter-Bootloader/cmake_gcc
)

ExternalProject_Add_StepDependencies(Matter-Bootloader build MatterElectricalMeterOverThread)


set(post_build_command ${POST_BUILD_EXE} postbuild "./MatterElectricalMeterSolution.slpb")


add_custom_target(postbuild ALL
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)

add_dependencies(postbuild MatterElectricalMeterOverThread Matter-Bootloader)

# BEGIN_SIMPLICITY_STUDIO_METADATA=eJy1U01P4zAQ/StWhFZwIFMWiUMJoF3KIVJC0LqVQIpUuc7QmnXsYDsEhPa/M1CaFlF2T3uc8Xvz3nz4OeJpfpWl5+n4ZsrHk1FaTHmRTcZpcTnNi9FFFg2j5Oyx1uwBnVfWnJTRQTwoI4ZG2kqZOSV+8PM0LaOz09IkObe6DQT0jFjGDx+9IsgihGYI0HVd3B3G1s3h+2BwANd5xuUCa7GvjA/CSKTKxBgaeylq9I2QuARkVoqw1N+o5ZUWMx9LW4P3sJKOUVpHlcgPY4l/z75FFK9Qua1ajb/wFh31gp6piop7LePOut9v0nElgigj6KmjfIS3yqhX/tf4dzgRGuvDrFW64gGbPk0P6+TOLrmvhanQ7bEez77dtzYc7+yu3O9BLkJAd6FRBqek0DlS2LfsdTNbchL4rJjAdidJ4+wdFWSGpk3dbNUoaPXjhUNR0XL0eg8w8XQTIBoj5k/AVd1oJVV44qGtlIWHo2k/mL+b3/66KQv/cLz/09qgraAh/l+PH4T6u4APh7E8O9i4uwTW34LC6M8Ljps6OA===END_SIMPLICITY_STUDIO_METADATA