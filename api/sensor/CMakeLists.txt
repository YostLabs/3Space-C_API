target_sources(TSS_Api
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/util.c
        ${CMAKE_CURRENT_LIST_DIR}/internal.h
        ${CMAKE_CURRENT_LIST_DIR}/commands.c
        ${CMAKE_CURRENT_LIST_DIR}/settings.c
        
        ${CMAKE_CURRENT_LIST_DIR}/base.c
        ${CMAKE_CURRENT_LIST_DIR}/sensor_managed.c
        ${CMAKE_CURRENT_LIST_DIR}/sensor_minimal.c
)