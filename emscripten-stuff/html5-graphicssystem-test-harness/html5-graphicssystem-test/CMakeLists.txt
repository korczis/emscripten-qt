project(HTML5RendererTest)
find_package(Qt4)

set(SRC main.cpp tests.cpp testdriver.cpp)
if (EMSCRIPTEN_NATIVE)
    set(SRC ${SRC} canvasinterface-emscripten-native.cpp)
endif()
qt4_wrap_cpp(SRC_MOC tests.h)
qt4_wrap_cpp(SRC_MOC testdriver.h)

include_directories(${QT_INCLUDES})

add_executable(html5renderertest ${SRC_MOC} ${SRC} )
target_link_libraries(html5renderertest ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})
