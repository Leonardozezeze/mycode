cmake_minimum_required(VERSION 3.19)
project(Serial_Wave LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets PrintSupport SerialPort)

qt_standard_project_setup()

qt_add_executable(Serial_Wave
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    configstatuspanel.h configstatuspanel.cpp
    wavepanel.h wavepanel.cpp
    samplearchive.h samplearchive.cpp
    theme.h
    qcustomplot.h qcustomplot.cpp
    serialpanel.h serialpanel.cpp
    logpanel.h logpanel.cpp
    commandbar.h commandbar.cpp
    statuspanel.h statuspanel.cpp
    recordingpanel.h recordingpanel.cpp
    protocolparser.h protocolparser.cpp
    machinestatus.h
    apptheme.h

)

target_link_libraries(Serial_Wave
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::PrintSupport
        Qt::SerialPort
)

install(TARGETS Serial_Wave
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Serial_Wave
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
