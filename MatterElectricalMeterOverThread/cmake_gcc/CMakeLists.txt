# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	MatterElectricalMeterOverThread
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_MatterElectricalMeterOverThread target,
# which contains the content of the SLC project
include(MatterElectricalMeterOverThread.cmake)

add_executable(MatterElectricalMeterOverThread
    # Add additional sources here
    "${CMAKE_SOURCE_DIR}/../src/PriceReporterManager.cpp"
)

target_include_directories(MatterElectricalMeterOverThread PUBLIC
    # Add additional include paths here
    "${CMAKE_SOURCE_DIR}/../include/PriceReportManager.h"
)

target_compile_definitions(MatterElectricalMeterOverThread PUBLIC
    # Add additional macros here
)

target_compile_options(MatterElectricalMeterOverThread PUBLIC
    # Set additional compiler flags here
)

target_link_options(MatterElectricalMeterOverThread PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(MatterElectricalMeterOverThread PRIVATE
    slc
)

list(REMOVE_ITEM SOURCES
     "${CMAKE_SOURCE_DIR}/../src/LightingManager.cpp"
     "${CMAKE_SOURCE_DIR}/../include/LightingManager.h"
)

# Include managed project content if available
include(MatterElectricalMeterOverThread_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(MatterElectricalMeterOverThread PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET MatterElectricalMeterOverThread
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:MatterElectricalMeterOverThread>" "$<TARGET_FILE_DIR:MatterElectricalMeterOverThread>/$<TARGET_FILE_BASE_NAME:MatterElectricalMeterOverThread>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:MatterElectricalMeterOverThread>" "$<TARGET_FILE_DIR:MatterElectricalMeterOverThread>/$<TARGET_FILE_BASE_NAME:MatterElectricalMeterOverThread>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:MatterElectricalMeterOverThread>" "$<TARGET_FILE_DIR:MatterElectricalMeterOverThread>/$<TARGET_FILE_BASE_NAME:MatterElectricalMeterOverThread>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET MatterElectricalMeterOverThread
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()