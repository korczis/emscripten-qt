#include "html5canvasinterface.h"

#ifndef EMSCRIPTEN_NATIVE
#define EMSCRIPTEN_NATIVE
/**
 * The native implementation is in emscripten-stuff/html5-graphicssystem-test-harness/html5-graphicssystem-test/
 * for now.
 */

extern "C"
{
    CanvasHandle EMSCRIPTENQT_handleForMainCanvas();
    CanvasHandle EMSCRIPTENQT_createCanvas(int width, int height);
    void EMSCRIPTENQT_fillSolidRect(CanvasHandle canvasHandle, int r, int g, int b, double x, double y, double width, double height);
    void EMSCRIPTENQT_drawCanvasOnMainCanvas(CanvasHandle canvasHandle, int x, int y);
    void EMSCRIPTENQT_clearMainCanvas(Rgba rgba);
    void EMSCRIPTENQT_mainCanvasContentsRaw_internal(void* destPtr);
    int EMSCRIPTENQT_canvas_width_pixels();
    int EMSCRIPTENQT_canvas_height_pixels();
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

void Html5CanvasInterface::clearMainCanvas(Rgba rgba)
{
    EMSCRIPTENQT_clearMainCanvas(rgba);
}

Rgba* Html5CanvasInterface::mainCanvasContentsRaw()
{
    const int numPixels = mainCanvasWidth() * mainCanvasHeight();
    void *destPointer = malloc(numPixels * sizeof(Rgba);
    EMSCRIPTENQT_mainCanvasContentsRaw_internal(destPointer);
    return static_cast<Rgba*>(destPointer);
}

#endif

#include <QtGui/qcolor.h>
#include <qdebug.h>

// Implementation is common to both native and non-native versions.
QImage Html5CanvasInterface::mainCanvasContents()
{
    const int CANVAS_WIDTH = mainCanvasWidth();
    const int CANVAS_HEIGHT = mainCanvasHeight();
    Rgba *canvasContentsRaw = mainCanvasContentsRaw();
    QImage canvasContentsImage(CANVAS_WIDTH, CANVAS_HEIGHT, QImage::Format_ARGB32);
    for (int y = 0; y < CANVAS_HEIGHT; y++)
    {
        for (int x = 0; x < CANVAS_WIDTH; x++)
        {
            const Rgba pixelRgba = canvasContentsRaw[x + y * CANVAS_WIDTH];
            qDebug() << "pixelRgba: " << pixelRgba;
            const int a = pixelRgba >> 24;
            const int b = (pixelRgba & 0xFF0000) >> 16;
            const int g =  (pixelRgba & 0xFF00) >> 8;
            const int r =   (pixelRgba & 0xFF) >> 0;
            qDebug() << "r: " << r << " g: " << g << " b: " << b << " a: " << a;
            canvasContentsImage.setPixel(x, y, QColor(r, g, b, a).rgba());
        }
    }
    free(canvasContentsRaw);
    return canvasContentsImage;
}
