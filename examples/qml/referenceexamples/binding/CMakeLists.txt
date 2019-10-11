# Generated from binding.pro.

cmake_minimum_required(VERSION 3.14)
project(binding LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(INSTALL_EXAMPLEDIR "examples")

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS Qml)

add_qt_gui_executable(binding
    birthdayparty.cpp birthdayparty.h
    happybirthdaysong.cpp happybirthdaysong.h
    main.cpp
    person.cpp person.h
)
target_link_libraries(binding PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Qml
)

# Resources:
set(binding_resource_files
    "example.qml"
)

qt6_add_resources(binding "binding"
    PREFIX
        "/"
    FILES
        ${binding_resource_files}
)


install(TARGETS binding
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)