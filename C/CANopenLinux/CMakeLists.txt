cmake_minimum_required(VERSION 3.16)
project(CANopenLinux C)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CANopenNode/CANopen.c)
    message(FATAL_ERROR "CANopenNode submodule is missing. Run: git submodule update --init")
endif()

# Application directory: user OD (OD.c/OD.h) and application hooks (app.c)
set(APPL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/app)

add_executable(canopend
    CO_driver.c
    CO_error.c
    CO_epoll_interface.c
    CO_storageLinux.c
    CANopenNode/301/CO_ODinterface.c
    CANopenNode/301/CO_NMT_Heartbeat.c
    CANopenNode/301/CO_HBconsumer.c
    CANopenNode/301/CO_Emergency.c
    CANopenNode/301/CO_SDOserver.c
    CANopenNode/301/CO_SDOclient.c
    CANopenNode/301/CO_TIME.c
    CANopenNode/301/CO_SYNC.c
    CANopenNode/301/CO_PDO.c
    CANopenNode/301/crc16-ccitt.c
    CANopenNode/301/CO_fifo.c
    CANopenNode/303/CO_LEDs.c
    CANopenNode/304/CO_GFC.c
    CANopenNode/304/CO_SRDO.c
    CANopenNode/305/CO_LSSslave.c
    CANopenNode/305/CO_LSSmaster.c
    CANopenNode/309/CO_gateway_ascii.c
    CANopenNode/storage/CO_storage.c
    CANopenNode/extra/CO_trace.c
    CANopenNode/CANopen.c
    ${APPL_SRC_DIR}/OD.c
    ${APPL_SRC_DIR}/app.c
    CO_main_basic.c
)

target_include_directories(canopend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/CANopenNode
    ${APPL_SRC_DIR}
)

target_compile_definitions(canopend PRIVATE
    CO_SINGLE_THREAD
    CO_USE_APPLICATION
)

target_compile_options(canopend PRIVATE -Wall -g)
target_link_options(canopend PRIVATE -g)

install(TARGETS canopend RUNTIME DESTINATION bin)

# cocomm: CiA 309-3 command-line client for the canopend gateway
add_executable(cocomm cocomm/cocomm.c)
target_include_directories(cocomm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cocomm)
target_compile_options(cocomm PRIVATE -Wall -g)
install(TARGETS cocomm RUNTIME DESTINATION bin)
