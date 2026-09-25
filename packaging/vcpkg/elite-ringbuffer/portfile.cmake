# LOCAL overlay, not a claim of public registry submission. The full source tree
# stays together; pin and verify its published SHA-256 before using this port.
get_filename_component(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT EXISTS "${SOURCE_PATH}/include/elite_ringbuffer.h" OR NOT EXISTS "${SOURCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "Use the overlay inside its complete authenticated source tree")
endif()
set(ELITE_STATIC OFF)
set(ELITE_SHARED OFF)
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    set(ELITE_STATIC ON)
else()
    set(ELITE_SHARED ON)
endif()
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DELITE_BUILD_STATIC=${ELITE_STATIC}
        -DELITE_BUILD_SHARED=${ELITE_SHARED}
        -DELITE_BUILD_TESTS=OFF
        -DELITE_BUILD_DEMO=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME elite_ringbuffer CONFIG_PATH lib/cmake/elite_ringbuffer)
vcpkg_fixup_pkgconfig()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include"
                    "${CURRENT_PACKAGES_DIR}/debug/share"
                    "${CURRENT_PACKAGES_DIR}/share/elite_ringbuffer")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE" "${SOURCE_PATH}/NOTICE.md")
