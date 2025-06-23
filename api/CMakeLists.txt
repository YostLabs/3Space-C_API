target_sources(TSS_Api
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/core.c
        ${CMAKE_CURRENT_LIST_DIR}/header.c
        ${CMAKE_CURRENT_LIST_DIR}/command_table.c
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/sensor)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/sys)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tools)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/com)