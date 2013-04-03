#ifndef EMSCRIPTEN_NATIVE
#define EMSCRIPTEN_NATIVE
/**
 * The native implementation is in emscripten-stuff/html5-graphicssystem-test-harness/html5-graphicssystem-test/
 * for now.
 */
#include "html5canvasinterface.h"

extern "C"
{
    CanvasHandle EMSCRIPTENQT_handleForMainCanvas();
    CanvasHandle EMSCRIPTENQT_createCanvas(int width, int height);
    void EMSCRIPTENQT_fillSolidRect(CanvasHandle canvasHandle, int r, int g, int b, double x, double y, double width, double height);
    void EMSCRIPTENQT_drawCanvasOnMainCanvas(CanvasHandle canvasHandle, int x, int y);
}

CanvasHandle Html5CanvasInterface::handleForMainCanvas()
{
    return EMSCRIPTENQT_handleForMainCanvas();
}

CanvasHandle Html5CanvasInterface::createCanvas(int width, int height)
{
    return EMSCRIPTENQT_createCanvas(width, height);
}

void Html5CanvasInterface::fillSolidRect(CanvasHandle canvasHandle, int r, int g, int b, double x, double y, double width, double height)
{
    EMSCRIPTENQT_fillSolidRect(canvasHandle, r, g, b, x, y, width, height);
}

void Html5CanvasInterface::drawCanvasOnMainCanvas(CanvasHandle canvasHandle, int x, int y)
{
    EMSCRIPTENQT_drawCanvasOnMainCanvas(canvasHandle, x, y);
}


#endif