cmake_minimum_required(VERSION 3.26)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Board type
set(BOARD_TYPE $ENV{BOARD_TYPE})
if(NOT BOARD_TYPE)
    set(BOARD_TYPE "pico")
endif()
set(PICO_BOARD ${BOARD_TYPE})

if("${BOARD_TYPE}" STREQUAL "pico")
    add_compile_definitions(BOARD_TYPE_PICO_W=0 BOARD_TYPE_PICO=1 BOARD_TYPE_CUSTOM16MB=0)
elseif("${BOARD_TYPE}" STREQUAL "pico_w")
    add_compile_definitions(BOARD_TYPE_PICO_W=1 BOARD_TYPE_PICO=0 BOARD_TYPE_CUSTOM16MB=0)
else()
    message(FATAL_ERROR "Unknown BOARD_TYPE: ${BOARD_TYPE}")
endif()

# Pico SDK
if(NOT DEFINED ENV{PICO_SDK_PATH})
    set(ENV{PICO_SDK_PATH} ../../pico-sdk)
endif()
message("PICO_SDK_PATH: $ENV{PICO_SDK_PATH}")

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(rp C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_subdirectory(settings)

# Test image mode (default off).
# If not set in cache, fall back to TEST_IMAGE_MODE environment variable.
set(_TEST_IMAGE_MODE_DEFAULT "0")
if(DEFINED ENV{TEST_IMAGE_MODE} AND NOT "$ENV{TEST_IMAGE_MODE}" STREQUAL "")
    set(_TEST_IMAGE_MODE_DEFAULT "$ENV{TEST_IMAGE_MODE}")
endif()

if(NOT DEFINED TEST_IMAGE_MODE)
    set(TEST_IMAGE_MODE "${_TEST_IMAGE_MODE_DEFAULT}" CACHE STRING "Enable static test image mode (0 or 1)")
endif()

if(TEST_IMAGE_MODE STREQUAL "1" OR TEST_IMAGE_MODE STREQUAL "ON" OR TEST_IMAGE_MODE STREQUAL "TRUE")
    set(TEST_IMAGE_MODE_ENABLED 1)
elseif(TEST_IMAGE_MODE STREQUAL "0" OR TEST_IMAGE_MODE STREQUAL "OFF" OR TEST_IMAGE_MODE STREQUAL "FALSE" OR TEST_IMAGE_MODE STREQUAL "")
    set(TEST_IMAGE_MODE_ENABLED 0)
else()
    message(FATAL_ERROR "Invalid TEST_IMAGE_MODE='${TEST_IMAGE_MODE}'. Use 0 or 1.")
endif()

message(STATUS "TEST_IMAGE_MODE: ${TEST_IMAGE_MODE_ENABLED}")

add_executable(${PROJECT_NAME}
    main.c
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/romemul.pio)

target_sources(${PROJECT_NAME} PRIVATE
    emul.c
    gconfig.c
    reset.c
    romemul.c
    select.c
    settings/settings.c
    vga.c
    vga_c2p.c
    vga_font.c
    chunk_data.c
    engine/render_camera.c
    engine/render_chunk.c
    engine/render_clipping.c
    engine/render_culling.c
    engine/render_lighting.c
    engine/render_model.c
    engine/render_rasterize.c
    engine/render_triangle.c
    game/logic_day_night_cycle.c
    game/logic_demo.c
    game/logic_events.c
    game/logic_gate.c
    game/logic_grass.c
    game/logic_info_text.c
    game/logic_input.c
    game/logic_menu.c
    game/logic_npc.c
    game/logic_physics.c
    game/logic_quest_npcs.c
    game/logic_random.c
    game/logic_shoot.c
    game/logic_zombies.c
)

pico_add_extra_outputs(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
    hardware_dma
    hardware_flash
    hardware_pio
    pico_stdlib
    pico_multicore
    settings
)

# Linker script with ROM_IN_RAM region
set_target_properties(${PROJECT_NAME} PROPERTIES
    PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/memmap_rp.ld
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/..
)

if(TEST_IMAGE_MODE_ENABLED)
    find_program(TEST_IMAGE_MODE_PYTHON_EXECUTABLE NAMES python3 REQUIRED)
    message(STATUS "TEST_IMAGE_MODE Python: ${TEST_IMAGE_MODE_PYTHON_EXECUTABLE}")

    set(TEST_MODE_IMAGE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../assets/test_mode.png")
    set(TEST_MODE_IMAGE_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../../tools/gen_test_mode_image.py")
    set(TEST_MODE_IMAGE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
    set(TEST_MODE_IMAGE_HEADER "${TEST_MODE_IMAGE_GENERATED_DIR}/test_mode_image.h")

    add_custom_command(
        OUTPUT "${TEST_MODE_IMAGE_HEADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_MODE_IMAGE_GENERATED_DIR}"
        COMMAND ${TEST_IMAGE_MODE_PYTHON_EXECUTABLE}
                "${TEST_MODE_IMAGE_SCRIPT}"
                --input "${TEST_MODE_IMAGE_SOURCE}"
                --output "${TEST_MODE_IMAGE_HEADER}"
        DEPENDS
            "${TEST_MODE_IMAGE_SCRIPT}"
            "${TEST_MODE_IMAGE_SOURCE}"
        COMMENT "Generating test mode image header from assets/test_mode.png"
        VERBATIM
    )

    add_custom_target(generate_test_mode_image_header DEPENDS "${TEST_MODE_IMAGE_HEADER}")
    add_dependencies(${PROJECT_NAME} generate_test_mode_image_header)
    target_sources(${PROJECT_NAME} PRIVATE "${TEST_MODE_IMAGE_HEADER}")
    target_include_directories(${PROJECT_NAME} PRIVATE "${TEST_MODE_IMAGE_GENERATED_DIR}")
    set_source_files_properties(emul.c PROPERTIES OBJECT_DEPENDS "${TEST_MODE_IMAGE_HEADER}")
    target_compile_definitions(${PROJECT_NAME} PRIVATE TEST_IMAGE_MODE=1)
endif()

# Release metadata
set(RELEASE_TYPE $ENV{RELEASE_TYPE})
set(RELEASE_VERSION $ENV{RELEASE_VERSION})
set(RELEASE_DATE $ENV{RELEASE_DATE})
set(_DEBUG $ENV{DEBUG_MODE})

if(NOT RELEASE_VERSION)
    if(NOT RELEASE_TYPE OR RELEASE_TYPE STREQUAL "")
        set(RELEASE_TYPE "final")
    endif()
    string(STRIP "${RELEASE_VERSION}" RELEASE_VERSION)
    if("${RELEASE_VERSION}" STREQUAL "")
        set(RELEASE_VERSION "v0.0.1dev")
    endif()
endif()

if(NOT RELEASE_DATE)
    string(TIMESTAMP CURRENT_DATE_TIME "%Y-%m-%d %H:%M:%S")
    set(RELEASE_DATE ${CURRENT_DATE_TIME})
endif()

if(NOT _DEBUG)
    set(_DEBUG 0)
endif()

pico_enable_stdio_usb(${PROJECT_NAME} 0)
if(${_DEBUG} STREQUAL "0")
    pico_enable_stdio_uart(${PROJECT_NAME} 0)
else()
    pico_enable_stdio_uart(${PROJECT_NAME} 1)
endif()

message("RELEASE_TYPE: " ${RELEASE_TYPE})
message("RELEASE_VERSION: " ${RELEASE_VERSION})
message("RELEASE_DATE: " ${RELEASE_DATE})
message("DEBUG_MODE: " ${_DEBUG})

add_definitions(-DRELEASE_TYPE="${RELEASE_TYPE}")
add_definitions(-DRELEASE_VERSION="${RELEASE_VERSION}")
add_definitions(-DRELEASE_DATE="${RELEASE_DATE}")

if(DEFINED ENV{APP_UUID_KEY} AND NOT "$ENV{APP_UUID_KEY}" STREQUAL "")
    message(STATUS "Using APP_UUID_KEY: $ENV{APP_UUID_KEY}")
    add_definitions(-DCURRENT_APP_UUID_KEY=\"$ENV{APP_UUID_KEY}\")
else()
    message(STATUS "Using default UUID")
    add_definitions(-DCURRENT_APP_UUID_KEY=\"44444444-4444-4444-8444-444444444444\")
endif()

add_definitions(-D_DEBUG=${_DEBUG})
add_definitions(-DDISPLAY_ATARIST)
add_definitions(-DPICO_FLASH_ASSUME_CORE0_SAFE=1)
add_definitions(-DNO_GLOBAL_OFFSET=0)

target_link_options(${PROJECT_NAME} PRIVATE
    "-Wl,--gc-sections"
    "-Wl,--strip-all"
)

set(PICO_BINARY_TYPE default)
pico_set_binary_type(${PROJECT_NAME} ${PICO_BINARY_TYPE})

set_target_properties(${PROJECT_NAME} PROPERTIES
    PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/memmap_rp.ld
)
