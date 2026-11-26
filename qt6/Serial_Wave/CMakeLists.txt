cmake_minimum_required(VERSION 3.19)
project(Serial_Wave VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets PrintSupport SerialPort SerialBus Concurrent)
qt_standard_project_setup()

add_library(serial_wave_backend STATIC
    protocolparser.h protocolparser.cpp machinestatus.h telemetry.h
    samplearchive.h samplearchive.cpp sessionio.h sessionio.cpp
    sessionevents.h sessionevents.cpp sessioncleanup.h sessioncleanup.cpp)
target_include_directories(serial_wave_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(serial_wave_backend PUBLIC Qt::Core)

add_library(serial_wave_ui STATIC
    mainwindow.h mainwindow.cpp
    configstatuspanel.h configstatuspanel.cpp
    wavepanel.h wavepanel.cpp wavepanel_analysis.cpp theme.h apptheme.h
    qcustomplot.h qcustomplot.cpp
    serialpanel.h serialpanel.cpp
    canpanel.h canpanel.cpp canframe.h
    logpanel.h logpanel.cpp
    uartterminal.h uartterminal.cpp
    commandbar.h commandbar.cpp
    statuspanel.h statuspanel.cpp
    recordingpanel.h recordingpanel.cpp)
target_link_libraries(serial_wave_ui PUBLIC serial_wave_backend Qt::Widgets Qt::PrintSupport Qt::SerialPort Qt::SerialBus Qt::Concurrent)
qt_add_executable(Serial_Wave WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(Serial_Wave PRIVATE serial_wave_ui)

if(APPLE)
    # Qt's peakcan plugin loads the bare name "PCBUSB" via dlopen().
    # MacCAN installs libPCBUSB.dylib, which that lookup does not find.
    find_library(PCBUSB_LIBRARY NAMES PCBUSB PATHS /usr/local/lib /opt/homebrew/lib)
    if(PCBUSB_LIBRARY)
        add_custom_command(TARGET Serial_Wave POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:Serial_Wave>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCBUSB_LIBRARY}"
                "$<TARGET_BUNDLE_DIR:Serial_Wave>/Contents/Frameworks/PCBUSB"
            VERBATIM)
        install(FILES "${PCBUSB_LIBRARY}"
            DESTINATION "Serial_Wave.app/Contents/Frameworks" RENAME PCBUSB)
    else()
        message(STATUS "MacCAN libPCBUSB not found; install it and reconfigure to enable PCAN on macOS")
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(serial_wave_tests tests/test_serial_wave.cpp)
    target_link_libraries(serial_wave_tests PRIVATE serial_wave_ui Qt::Test)
    add_test(NAME serial_wave_tests COMMAND serial_wave_tests)
    set_tests_properties(serial_wave_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS Serial_Wave
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
qt_generate_deploy_app_script(TARGET Serial_Wave OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
