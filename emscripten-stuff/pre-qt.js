var EMSCRIPTENQT_callbackTimer = null;
function _EMSCRIPTENQT_timerCallback_springboard()
{
	try
	{
		_EMSCRIPTENQT_timerCallback();
		Module.print("Completed without exceptions");
	}
	catch (e)
	{
		window.alert("Exception occurred in event handler: " + e + "-" + e.stack);
		//window.alert("Exception occurred in event handler: " + e);
	}
}
function _EMSCRIPTENQT_resetTimerCallback(milliseconds)
{
	if (EMSCRIPTENQT_callbackTimer != null)
	{
		clearTimeout(EMSCRIPTENQT_callbackTimer);
		EMSCRIPTENQT_callbackTimer = null;
	}
	EMSCRIPTENQT_callbackTimer = setTimeout(_EMSCRIPTENQT_timerCallback_springboard, milliseconds);
	Module.print("callback requested in " + milliseconds + "ms");
}
function _EMSCRIPTEN_canvas_width_pixels()
{
	var canvas = document.getElementById('canvas');
	return canvas.width;
}
function _EMSCRIPTEN_canvas_height_pixels()
{
	var canvas = document.getElementById('canvas');
	return canvas.height;
}
function _EMSCRIPTEN_flush_pixels(data)
{
        var canvasWidth = _EMSCRIPTEN_canvas_width_pixels();
        var canvasHeight = _EMSCRIPTEN_canvas_height_pixels();
        var numPixels = canvasWidth * canvasHeight;
        var canvas = document.getElementById('canvas');
        var canvasContext = canvas.getContext("2d");
        var imageData = canvasContext.createImageData(canvasWidth, canvasHeight);
        var imageDataPos = 0;
        Module.print("Flush pixels: " + canvasWidth + "x" + canvasHeight);
        while (numPixels != 0)
        {
                var argb = HEAP32[((data)>>2)];
                imageData.data[imageDataPos] = (argb & 0x00FF0000) >> 16;
                imageDataPos++;
                imageData.data[imageDataPos] = (argb & 0x0000FF00) >> 8;
                imageDataPos++;
                imageData.data[imageDataPos] = (argb & 0X000000FF) >> 0;
                imageDataPos++;
                imageData.data[imageDataPos] = 255;
                imageDataPos++;
                data = data + 4;
                numPixels--;
        }
        canvasContext.putImageData(imageData, 0, 0);
}
function EMSCRIPTENQT_mouseMoved(e)
{
        var canvas = document.getElementById('canvas');
        Module.print("Mouse moved: " + (e.pageX - canvas.offsetLeft) + "," + (e.pageY - canvas.offsetTop));
	cwrap('EMSCRIPTENQT_mouseCanvasPosChanged', 'number', ['number', 'number'])(e.pageX - canvas.offsetLeft, e.pageY - canvas.offsetTop);
}
function EMSCRIPTENQT_mouseDown(e)
{
        var canvas = document.getElementById('canvas');
        cwrap('EMSCRIPTENQT_mouseCanvasButtonChanged', 'number', ['number', 'number'])(1, 1);
}
function EMSCRIPTENQT_mouseUp(e)
{
        var canvas = document.getElementById('canvas');
        cwrap('EMSCRIPTENQT_mouseCanvasButtonChanged', 'number', ['number', 'number'])(1, 0);
}
Module.noExitRuntime = true;
Module['preRun'] = function() {
	try
	{
                var canvas = document.getElementById('canvas');
                canvas.addEventListener("mousemove", EMSCRIPTENQT_mouseMoved, false);
                canvas.addEventListener("mousedown", EMSCRIPTENQT_mouseDown, false);
                canvas.addEventListener("mouseup", EMSCRIPTENQT_mouseUp, false);
		// Data cache dir for QWS
		Module['FS_createFolder']("/tmp/", 'qtembedded-0', true, true);

	} catch (e)
	{
		window.alert("Exception while setting up qws filesystem: " + e);
	}
}

