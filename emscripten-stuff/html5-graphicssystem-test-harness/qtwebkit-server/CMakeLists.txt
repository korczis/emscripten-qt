project(QtWebkit-Server)
find_package(Qt4)

set(SRC main.cpp commandlistener.cpp)
qt4_wrap_cpp(SRC_MOC commandlistener.h)

include_directories(${QT_INCLUDES})

add_executable(qtwebkitserver ${SRC} ${SRC_MOC})
target_link_libraries(qtwebkitserver ${QT_QTWEBKIT_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTCORE_LIBRARY})
