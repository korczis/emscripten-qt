project(HTML5RendererTest)
find_package(Qt4)

set(SRC main.cpp tests.cpp testdriver.cpp testwidget.cpp)
qt4_wrap_cpp(SRC_MOC tests.h)
qt4_wrap_cpp(SRC_MOC testdriver.h)
qt4_wrap_cpp(SRC_MOC testwidget.h)

include_directories(${QT_INCLUDES})

add_definitions(-DEXPECTED_TEST_IMAGES_DIR="\\\"${CMAKE_SOURCE_DIR}/expected-test-images/\\\"")
add_definitions(-DTEST_DATA_DIR="\\\"${CMAKE_SOURCE_DIR}/testdata/\\\"")

add_executable(html5renderertest ${SRC_MOC} ${SRC} )
target_link_libraries(html5renderertest ${QT_QTGUI_LIBRARY} ${QT_QTCORE_LIBRARY})

if (EMSCRIPTEN_NATIVE)
    # Unfortunately, we can't use FindSDL as we're working with a kind of "CMake chroot", so feed in the SDL
    # includes and libraries manually :/
    include_directories("/usr/include/SDL")
    target_link_libraries(html5renderertest "-lSDL_net")
endif()
