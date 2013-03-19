project(QtWebkit-Server)
find_package(Qt4)

set(SRC main.cpp)

include_directories(${QT_INCLUDES})

add_executable(qtwebkitserver ${SRC})
target_link_libraries(qtwebkitserver ${QT_QTWEBKIT_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
