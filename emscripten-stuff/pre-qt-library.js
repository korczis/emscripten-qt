mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_mainLoopInitialised: function() {
          return _EMSCRIPTENQT_mainLoopInitialised_internal();
      }
    });


mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_resetTimerCallback: function(ms) {
            _EMSCRIPTENQT_resetTimerCallback_internal(ms);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_cursorChanged: function(newCursorShape) {
            _EMSCRIPTENQT_cursorChanged_internal(newCursorShape);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_launchWebBrowser: function(urlCString) {
            _EMSCRIPTENQT_launchWebBrowser_internal(urlCString);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_canvas_width_pixels: function() {
            return _EMSCRIPTENQT_canvas_width_pixels_internal();
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_canvas_height_pixels: function() {
            return _EMSCRIPTENQT_canvas_height_pixels_internal();
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_flush_pixels: function(data, regionX, regionY, regionW, regionH) {
            _EMSCRIPTENQT_flush_pixels_internal(data, regionX, regionY, regionW, regionH);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_createCanvas: function(width, height) {
          return _EMSCRIPTENQT_createCanvas_internal(width, height);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_fillSolidRect: function(canvasHandle, r, g, b, x, y, width, height) {
          return _EMSCRIPTENQT_fillSolidRect_internal(canvasHandle, r, g, b, x, y, width, height);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_strokeRect: function(canvasHandle, x, y, width, height) {
          return _EMSCRIPTENQT_strokeRect_internal(canvasHandle, x, y, width, height);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_fillRect: function(canvasHandle, x, y, width, height) {
          return _EMSCRIPTENQT_fillRect_internal(canvasHandle, x, y, width, height);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_strokeEllipse: function(canvasHandle, cx, cy, width, height) {
          return _EMSCRIPTENQT_strokeEllipse_internal(canvasHandle, cx, cy, width, height);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_fillEllipse: function(canvasHandle, cx, cy, width, height) {
          return _EMSCRIPTENQT_fillEllipse_internal(canvasHandle, cx, cy, width, height);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_drawLine: function(canvasHandle, startX, startY, endX, endY) {
          return _EMSCRIPTENQT_drawLine_internal(canvasHandle, startX, startY, endX, endY);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_changePenColor: function(canvasHandle, r, g, b) {
          return _EMSCRIPTENQT_changePenColor_internal(canvasHandle, r, g, b);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_changePenThickness: function(canvasHandle, thickness) {
          return _EMSCRIPTENQT_changePenThickness_internal(canvasHandle, thickness);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_changeBrushColor: function(canvasHandle, r, g, b) {
          return _EMSCRIPTENQT_changeBrushColor_internal(canvasHandle, r, g, b);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_changeBrushTexture: function(canvasHandle, r, g, b) {
          return _EMSCRIPTENQT_changeBrushTexture_internal(canvasHandle, r, g, b);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_createLinearGradient: function(canvasHandle, startX, startY, endX, endY) {
          return _EMSCRIPTENQT_createLinearGradient_internal(canvasHandle, startX, startY, endX, endY);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_addStopPointToCurrentGradient: function(position, r, g, b) {
          return _EMSCRIPTENQT_addStopPointToCurrentGradient_internal(position, r, g, b);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_setBrushToCurrentGradient: function(canvasHandle) {
          return _EMSCRIPTENQT_setBrushToCurrentGradient_internal(canvasHandle);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_savePaintState: function(canvasHandle) {
          return _EMSCRIPTENQT_savePaintState_internal(canvasHandle);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_restorePaintState: function(canvasHandle) {
          return _EMSCRIPTENQT_restorePaintState_internal(canvasHandle);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_restoreToOriginalState: function(canvasHandle) {
          return _EMSCRIPTENQT_restoreToOriginalState_internal(canvasHandle);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_setClipRect: function(canvasHandle, x, y, width, height) {
          return _EMSCRIPTENQT_setClipRect_internal(canvasHandle, x, y, width, height);
      }
    });

mergeInto(LibraryManager.library, {
     EMSCRIPTENQT_removeClip: function (canvasHandle) {
          return _EMSCRIPTENQT_removeClip_internal(canvasHandle);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_beginPath: function(canvasHandle) {
          return _EMSCRIPTENQT_beginPath_internal(canvasHandle);
      }
    });

mergeInto(LibraryManager.library, {
     EMSCRIPTENQT_currentPathMoveTo: function(x, y) {
         return _EMSCRIPTENQT_currentPathMoveTo_internal(x, y);
      }
    });

mergeInto(LibraryManager.library, {
     EMSCRIPTENQT_currentPathLineTo: function(x, y) {
         return _EMSCRIPTENQT_currentPathLineTo_internal(x, y);
      }
    });

mergeInto(LibraryManager.library, {
     EMSCRIPTENQT_currentPathCubicTo: function(control1X, control1Y, control2X, control2Y, endX, endY) {
         return _EMSCRIPTENQT_currentPathCubicTo_internal(control1X, control1Y, control2X, control2Y, endX, endY);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_addRectToCurrentPath: function(x, y, width, height) {
          return _EMSCRIPTENQT_addRectToCurrentPath_internal(x, y, width, height);
      }
    });
mergeInto(LibraryManager.library, {
       EMSCRIPTENQT_setClipToCurrentPath: function() {
          return _EMSCRIPTENQT_setClipToCurrentPath_internal();
     }
   });

mergeInto(LibraryManager.library, {
     EMSCRIPTENQT_strokeCurrentPath: function() {
         return _EMSCRIPTENQT_strokeCurrentPath_internal();
      }
    });

mergeInto(LibraryManager.library, {
     EMSCRIPTENQT_fillCurrentPath: function() {
         return _EMSCRIPTENQT_fillCurrentPath_internal();
      }
    });


mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_setTransform: function(canvasHandle, a, b, c, d, e, f) {
          return _EMSCRIPTENQT_setTransform_internal(canvasHandle, a, b, c, d, e, f);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_setCanvasPixelsRaw: function(canvasHandle, sourcePointer, width, height) {
          // Ignore width and height - they are currently used only by emscripten-native, and I hope to deprecate them, soon.
          return _EMSCRIPTENQT_setCanvasPixelsRaw_internal(canvasHandle, sourcePointer);
      }
    });


mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_drawCanvasOnCanvas: function(canvasHandleToDraw, canvasHandleToDrawOn, x, y) {
          return _EMSCRIPTENQT_drawCanvasOnCanvas_internal(canvasHandleToDraw, canvasHandleToDrawOn, x, y);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_drawStretchedCanvasPortionOnCanvas: function(canvasHandleToDraw, canvasHandleToDrawOn, targetX, targetY, targetWidth, targetHeight, sourceX, sourceY, sourceWidth, sourceHeight) {
          return _EMSCRIPTENQT_drawStretchedCanvasPortionOnCanvas_internal(canvasHandleToDraw, canvasHandleToDrawOn, targetX, targetY, targetWidth, targetHeight, sourceX, sourceY, sourceWidth, sourceHeight);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_drawCanvasOnMainCanvas: function(canvasHandle, x, y) {
          return _EMSCRIPTENQT_drawCanvasOnMainCanvas_internal(canvasHandle, x, y);
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_clearMainCanvas: function(rgba) {
          return _EMSCRIPTENQT_clearMainCanvas_internal(rgba);
      }
    });


mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_handleForMainCanvas: function() {
          return _EMSCRIPTENQT_handleForMainCanvas_internal();
      }
    });

mergeInto(LibraryManager.library, {
      EMSCRIPTENQT_notify_frame_rendered: function() {
          return _EMSCRIPTENQT_notify_frame_rendered_internal();
      }
    });
