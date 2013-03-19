project(HTML5RendererTest)
find_package(Qt4)

set(SRC main.cpp)

include_directories(${QT_INCLUDES})

add_executable(html5renderertest ${SRC})
target_link_libraries(html5renderertest ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
