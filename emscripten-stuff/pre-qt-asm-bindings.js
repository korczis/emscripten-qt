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
