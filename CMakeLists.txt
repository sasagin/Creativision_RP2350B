# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD weact_studio_rp2350b_core CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(CreatiVision C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(CreatiVision main.c )

pico_set_program_name(CreatiVision "CreatiVision")
pico_set_program_version(CreatiVision "0.1")

add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/fatfs)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(CreatiVision 0)
pico_enable_stdio_usb(CreatiVision 1)

pico_generate_pio_header(${PROJECT_NAME}
        ${CMAKE_CURRENT_SOURCE_DIR}/VDP_int.pio
        OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_compile_definitions(${PROJECT_NAME} PRIVATE

        SDCARD_PIN_SPI0_MISO=40
        SDCARD_PIN_SPI0_CS=41
        SDCARD_PIN_SPI0_SCK=42
        SDCARD_PIN_SPI0_MOSI=43
)

# Add the standard library to the build
target_link_libraries(CreatiVision
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(CreatiVision PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(CreatiVision

        sdcard
        fatfs

        pico_runtime
        pico_stdlib
        hardware_vreg
        hardware_clocks
        hardware_pio
        hardware_pwm
        pico_multicore
        hardware_interp hardware_timer  hardware_flash
        )

pico_add_extra_outputs(CreatiVision)

