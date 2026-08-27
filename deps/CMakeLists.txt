set(ZLIB_BUILD_STATIC ON)
set(LUAPP_SHARED ON)
project(lar_deps)

add_subdirectory(${PROJECT_SOURCE_DIR}/zlib)
add_subdirectory(${PROJECT_SOURCE_DIR}/libzip)
add_subdirectory(${PROJECT_SOURCE_DIR}/luapp)
add_library(Lar_Deps INTERFACE)

target_link_libraries(Lar_Deps INTERFACE
    zlibstatic
    zip
    Luapp
)