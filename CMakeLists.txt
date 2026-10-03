cmake_minimum_required(VERSION 3.16)
project(osal_freertos_port C)

set(FREERTOS_HEAP 4 CACHE STRING "")

set(OSAL_SYSTEM_OSTYPE freertos)
set(OSAL_SYSTEM_BSPTYPE generic-freertos)
set(OSAL_EXT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "")
# The littlefs volume's storage, supplied from outside OSAL as a target that
# defines OS_BSP_Storage_GetConfig() (see generic-freertos/inc/bsp-storage.h).
# The default is the POSIX simulator's RAM device, a test fixture.
set(OSAL_FREERTOS_STORAGE_TARGET osal_storage_posix_ram CACHE STRING "")
set(ENABLE_UNIT_TESTS TRUE CACHE BOOL "")

enable_testing()

add_subdirectory(third_party/osal osal_build)
add_subdirectory(third_party/FreeRTOS-Kernel freertos_kernel)

if (OSAL_FREERTOS_STORAGE_TARGET STREQUAL "osal_storage_posix_ram")
    add_library(osal_storage_posix_ram STATIC test-fixtures/storage-posix-ram/storage.c)
    target_link_libraries(osal_storage_posix_ram PUBLIC osal_littlefs)
    target_include_directories(osal_storage_posix_ram PRIVATE generic-freertos/inc)
endif ()

# The OS layer calls OS_BSP_Storage_GetConfig(), so every executable linking
# OSAL also links the storage target that defines it
target_link_libraries(osal PUBLIC ${OSAL_FREERTOS_STORAGE_TARGET})

# osal_bsp only picks up the BSP's compiled objects via $<TARGET_OBJECTS:...>,
# which does not inherit that target's own link to freertos_kernel - so it is
# linked here, otherwise anything linking osal_bsp fails with undefined
# references to FreeRTOS task/queue/scheduler symbols.
target_link_libraries(osal_bsp PUBLIC freertos_kernel)

if (ENABLE_UNIT_TESTS)
    add_subdirectory(freertos-coverage-tests)
endif ()