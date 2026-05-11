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

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(BUILD_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/build_version.h")

if(NOT EXISTS "${BUILD_VERSION_HEADER}")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(WRITE "${BUILD_VERSION_HEADER}"
"/* Auto-generated bootstrap header */
#ifndef BUILD_VERSION_H
#define BUILD_VERSION_H
#define BUILD_RELEASE_VERSION \"v0.0.0\"
#endif /* BUILD_VERSION_H */
")
endif()

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

add_custom_target(update_build_version ALL
    COMMAND "${REPO_ROOT}/tools/bump_version.sh"
            --repo-root "${REPO_ROOT}"
            --header "${BUILD_VERSION_HEADER}"
    BYPRODUCTS "${BUILD_VERSION_HEADER}"
    COMMENT "Updating build version metadata"
    VERBATIM
)
add_dependencies(${PROJECT_NAME} update_build_version)

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
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

if(TEST_IMAGE_MODE_ENABLED)
    set(TEST_MODE_IMAGE_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/test_mode_image.h")
    if(NOT EXISTS "${TEST_MODE_IMAGE_HEADER}")
        message(FATAL_ERROR
            "TEST_IMAGE_MODE=1 requires ${TEST_MODE_IMAGE_HEADER}. "
            "Generate it with: python3 tools/gen_test_mode_image.py "
            "--input assets/test_mode.png --output rp/src/include/test_mode_image.h")
    endif()
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
    set(RELEASE_VERSION "BUILD_RELEASE_VERSION")
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
add_definitions(-DRELEASE_VERSION=BUILD_RELEASE_VERSION)
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
