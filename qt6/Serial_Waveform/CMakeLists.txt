cmake_minimum_required(VERSION 3.16)
project(Serial_Waveform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SerialManager.cpp
    src/SerialManager.h
    src/WaveformWidget.cpp
    src/WaveformWidget.h
    src/FilterEngine.cpp
    src/FilterEngine.h
    src/ParameterTrendWidget.cpp
    src/ParameterTrendWidget.h
    src/ParameterTrendWindow.cpp
    src/ParameterTrendWindow.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::SerialPort
)
