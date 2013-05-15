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
    void EMSCRIPTENQT_strokeRect(CanvasHandle canvasHandle, double x, double y, double width, double height);
    void EMSCRIPTENQT_fillRect(CanvasHandle canvasHandle, double x, double y, double width, double height);
    void EMSCRIPTENQT_strokeEllipse(CanvasHandle canvasHandle, double cx, double cy, double width, double height);
    void EMSCRIPTENQT_fillEllipse(CanvasHandle canvasHandle, double cx, double cy, double width, double height);
    void EMSCRIPTENQT_drawLine(CanvasHandle canvasHandle, double startX, double startY, double endX, double endY);
    void EMSCRIPTENQT_changePenColor(CanvasHandle canvasHandle, int r, int g, int b);
    void EMSCRIPTENQT_changeBrushColor(CanvasHandle canvasHandle, int r, int g, int b);
    void EMSCRIPTENQT_changeBrushTexture(CanvasHandle canvasHandle, CanvasHandle textureHandle);
    void EMSCRIPTENQT_changePenThickness(CanvasHandle canvasHandle, double thickness);
    void EMSCRIPTENQT_createLinearGradient(CanvasHandle canvasHandle, double startX, double startY, double endX, double endY);
    void EMSCRIPTENQT_addStopPointToCurrentGradient(double position, int r, int g, int b);
    void EMSCRIPTENQT_setBrushToCurrentGradient(CanvasHandle canvasHandle);
    void EMSCRIPTENQT_savePaintState(CanvasHandle canvasHandle);
    void EMSCRIPTENQT_restorePaintState(CanvasHandle canvasHandle);
    void EMSCRIPTENQT_restoreToOriginalState(CanvasHandle canvasHandle);
    void EMSCRIPTENQT_setClipRect(CanvasHandle canvasHandle, double x, double y, double width, double height);
    void EMSCRIPTENQT_removeClip(CanvasHandle canvasHandle);
    void EMSCRIPTENQT_beginPath(CanvasHandle canvasHandle);
    void EMSCRIPTENQT_currentPathMoveTo(double x, double y);
    void EMSCRIPTENQT_currentPathLineTo(double x, double y);
    void EMSCRIPTENQT_currentPathCubicTo(double context1X, double context1Y, double context2X, double context2Y, double endX, double endY);
    void EMSCRIPTENQT_addRectToCurrentPath(double x, double y, double width, double height);
    void EMSCRIPTENQT_setClipToCurrentPath();
    void EMSCRIPTENQT_strokeCurrentPath();
    void EMSCRIPTENQT_fillCurrentPath();
    void EMSCRIPTENQT_setTransform(CanvasHandle canvasHandle, double a, double b, double c, double d, double e, double f);
    void EMSCRIPTENQT_setCanvasPixelsRaw(CanvasHandle canvasHandle, uchar* rgbaData, int width, int height);
    void EMSCRIPTENQT_drawCanvasOnMainCanvas(CanvasHandle canvasHandle, int x, int y);
    void EMSCRIPTENQT_drawCanvasOnCanvas(CanvasHandle canvasHandleToDraw,CanvasHandle canvasHandleToDrawOn, double x, double y);
    void EMSCRIPTENQT_drawStretchedCanvasPortionOnCanvas(CanvasHandle canvasHandleToDraw, CanvasHandle canvasHandleToDrawOn, double targetX, double targetY, double targetWidth, double targetHeight, double sourceX, double sourceY, double sourceWidth, double sourceHeight);
    void EMSCRIPTENQT_clearMainCanvas(Rgba rgba);
    void EMSCRIPTENQT_mainCanvasContentsRaw(void* destPtr);
    int EMSCRIPTENQT_canvas_width_pixels();
    int EMSCRIPTENQT_canvas_height_pixels();
    void EMSCRIPTENQT_loadFont(const char* fontData, int fontDataSize, char* familyName);
}


CanvasHandle Html5CanvasInterface::handleForMainCanvas()
{
    return EMSCRIPTENQT_handleForMainCanvas();
}

int Html5CanvasInterface::mainCanvasWidth()
{
    return EMSCRIPTENQT_canvas_width_pixels();
}

int Html5CanvasInterface::mainCanvasHeight()
{
    return EMSCRIPTENQT_canvas_height_pixels();
}

CanvasHandle Html5CanvasInterface::createCanvas(int width, int height)
{
    return EMSCRIPTENQT_createCanvas(width, height);
}

void Html5CanvasInterface::fillSolidRect(CanvasHandle canvasHandle, int r, int g, int b, double x, double y, double width, double height)
{
    EMSCRIPTENQT_fillSolidRect(canvasHandle, r, g, b, x, y, width, height);
}

void Html5CanvasInterface::strokeRect(CanvasHandle canvasHandle, double x, double y, double width, double height)
{
    EMSCRIPTENQT_strokeRect(canvasHandle, x, y, width, height);
}

void Html5CanvasInterface::fillRect(CanvasHandle canvasHandle, double x, double y, double width, double height)
{
    EMSCRIPTENQT_fillRect(canvasHandle, x, y, width, height);
}

void Html5CanvasInterface::strokeEllipse(CanvasHandle canvasHandle, double cx, double cy, double width, double height)
{
    EMSCRIPTENQT_strokeEllipse(canvasHandle, cx, cy, width, height);
}

void Html5CanvasInterface::fillEllipse(CanvasHandle canvasHandle, double cx, double cy, double width, double height)
{
    EMSCRIPTENQT_fillEllipse(canvasHandle, cx, cy, width, height);
}

void Html5CanvasInterface::drawLine(CanvasHandle canvasHandle, double startX, double startY, double endX, double endY)
{
    EMSCRIPTENQT_drawLine(canvasHandle, startX, startY, endX, endY);
}

void Html5CanvasInterface::changePenColor(CanvasHandle canvasHandle, int r, int g, int b)
{
    EMSCRIPTENQT_changePenColor(canvasHandle, r, g, b);
}

void Html5CanvasInterface::changeBrushColor(CanvasHandle canvasHandle, int r, int g, int b)
{
    EMSCRIPTENQT_changeBrushColor(canvasHandle, r, g, b);
}

void Html5CanvasInterface::changeBrushTexture(CanvasHandle canvasHandle, CanvasHandle textureHandle)
{
    EMSCRIPTENQT_changeBrushTexture(canvasHandle, textureHandle);
}

void Html5CanvasInterface::changePenThickness(CanvasHandle canvasHandle, double thickness)
{
    EMSCRIPTENQT_changePenThickness(canvasHandle, thickness);
}

void Html5CanvasInterface::createLinearGradient(CanvasHandle canvasHandle, double startX, double startY, double endX, double endY)
{
    EMSCRIPTENQT_createLinearGradient(canvasHandle, startX, startY, endX, endY);
}

void Html5CanvasInterface::addStopPointToCurrentGradient(double position, int r, int g, int b)
{
    EMSCRIPTENQT_addStopPointToCurrentGradient(position, r, g, b);
}

void Html5CanvasInterface::setBrushToCurrentGradient(CanvasHandle canvasHandle)
{
    EMSCRIPTENQT_setBrushToCurrentGradient(canvasHandle);
}

void Html5CanvasInterface::savePaintState(CanvasHandle canvasHandle)
{
    EMSCRIPTENQT_savePaintState(canvasHandle);
}

void Html5CanvasInterface::restorePaintState(CanvasHandle canvasHandle)
{
    EMSCRIPTENQT_restorePaintState(canvasHandle);
}

void Html5CanvasInterface::restoreToOriginalState(CanvasHandle canvasHandle)
{
    EMSCRIPTENQT_restoreToOriginalState(canvasHandle);
}

void Html5CanvasInterface::setClipRect(CanvasHandle canvasHandle, double x, double y, double w, double h)
{
    EMSCRIPTENQT_setClipRect(canvasHandle, x, y, w, h);
}

void Html5CanvasInterface::removeClip(CanvasHandle canvasHandle)
{
    EMSCRIPTENQT_removeClip(canvasHandle);
}

void Html5CanvasInterface::beginPath(CanvasHandle canvasHandle)
{
    EMSCRIPTENQT_beginPath(canvasHandle);
}

void Html5CanvasInterface::currentPathMoveTo(double x, double y)
{
    EMSCRIPTENQT_currentPathMoveTo(x, y);
}

void Html5CanvasInterface::currentPathLineTo(double x, double y)
{
    EMSCRIPTENQT_currentPathLineTo(x, y);
}

void Html5CanvasInterface::currentPathCubicTo(double control1X, double control1Y, double control2X, double control2Y, double endX, double endY)
{
    EMSCRIPTENQT_currentPathCubicTo(control1X, control1Y, control2X, control2Y, endX, endY);
}

void Html5CanvasInterface::addRectToCurrentPath(double x, double y, double width, double height)
{
    EMSCRIPTENQT_addRectToCurrentPath(x, y, width, height);
}

void Html5CanvasInterface::setClipToCurrentPath()
{
    EMSCRIPTENQT_setClipToCurrentPath();
}

void Html5CanvasInterface::strokeCurrentPath()
{
    EMSCRIPTENQT_strokeCurrentPath();
}

void Html5CanvasInterface::fillCurrentPath()
{
    EMSCRIPTENQT_fillCurrentPath();
}

void Html5CanvasInterface::setTransform(CanvasHandle canvasHandle, double a, double b, double c, double d, double e, double f)
{
    EMSCRIPTENQT_setTransform(canvasHandle, a, b, c, d, e, f);
}

void Html5CanvasInterface::setCanvasPixelsRaw(CanvasHandle canvasHandle, uchar* rgbaData, int width, int height)
{
    EMSCRIPTENQT_setCanvasPixelsRaw(canvasHandle, rgbaData, width, height);
}

void Html5CanvasInterface::drawCanvasOnMainCanvas(CanvasHandle canvasHandle, int x, int y)
{
    EMSCRIPTENQT_drawCanvasOnMainCanvas(canvasHandle, x, y);
}

void Html5CanvasInterface::drawCanvasOnCanvas(CanvasHandle canvasHandleToDraw, CanvasHandle canvasHandleToDrawOn, double x, double y)
{
    EMSCRIPTENQT_drawCanvasOnCanvas(canvasHandleToDraw, canvasHandleToDrawOn, x, y);
}


void Html5CanvasInterface::drawStretchedCanvasPortionOnCanvas(CanvasHandle canvasHandleToDraw, CanvasHandle canvasHandleToDrawOn, double targetX, double targetY, double targetWidth, double targetHeight, double sourceX, double sourceY, double sourceWidth, double sourceHeight)
{
    EMSCRIPTENQT_drawStretchedCanvasPortionOnCanvas(canvasHandleToDraw, canvasHandleToDrawOn, targetX, targetY, targetWidth, targetHeight, sourceX, sourceY, sourceWidth, sourceHeight);
}

void Html5CanvasInterface::clearMainCanvas(Rgba rgba)
{
    EMSCRIPTENQT_clearMainCanvas(rgba);
}

Rgba* Html5CanvasInterface::mainCanvasContentsRaw()
{
    const int numPixels = mainCanvasWidth() * mainCanvasHeight();
    void *destPointer = malloc(numPixels * sizeof(Rgba));
    EMSCRIPTENQT_mainCanvasContentsRaw(destPointer);
    return static_cast<Rgba*>(destPointer);
}

void Html5CanvasInterface::loadFont(const char* fontData, int fontDataSize, const char* familyName)
{
    EMSCRIPTENQT_loadFont(fontData, fontDataSize, familyName);
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
            const int a = pixelRgba >> 24;
            const int b = (pixelRgba & 0xFF0000) >> 16;
            const int g =  (pixelRgba & 0xFF00) >> 8;
            const int r =   (pixelRgba & 0xFF) >> 0;
            canvasContentsImage.setPixel(x, y, QColor(r, g, b, a).rgba());
        }
    }
    free(canvasContentsRaw);
    return canvasContentsImage;
}
