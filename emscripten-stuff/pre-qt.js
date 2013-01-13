try
{
        if (Float64Array == null)
        {
                throw "";
        }
}
catch (e)
{
        window.alert( "Javascript typed arrays apparently are not supported by your browser: unfortunately, emscripten-qt apps cannot run it it.");
        throw e;
}

var EMSCRIPTENQT_callbackTimer = null;
function _EMSCRIPTENQT_timerCallback_springboard()
{
	try
	{
		_EMSCRIPTENQT_timerCallback();
		//Module.print("Completed without exceptions");
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
	//Module.print("callback requested in " + milliseconds + "ms");
}
function _EMSCRIPTENQT_canvas_width_pixels()
{
	var canvas = document.getElementById('canvas');
	return canvas.width;
}
function _EMSCRIPTENQT_canvas_height_pixels()
{
	var canvas = document.getElementById('canvas');
	return canvas.height;
}
function _EMSCRIPTENQT_flush_pixels(data, regionX, regionY, regionW, regionH)
{
        var canvasWidth = _EMSCRIPTENQT_canvas_width_pixels();
        var canvasHeight = _EMSCRIPTENQT_canvas_height_pixels();
        var canvas = document.getElementById('canvas');
        var canvasContext = canvas.getContext("2d");
        var imageData = canvasContext.createImageData(regionW, regionH);
        var imageDataPos = 0;
        var dataIncAfterRow = (canvasWidth - regionW);
        var sourceData = (data >> 2) + canvasWidth * regionY + regionX;
        for (var y = 0; y < regionH; y++)
        {
                for (var x = 0; x < regionW; x++)
                {
                        var argb = HEAP32[sourceData];
                        imageData.data[imageDataPos] = (argb & 0x00FF0000) >> 16;
                        imageDataPos++;
                        imageData.data[imageDataPos] = (argb & 0x0000FF00) >> 8;
                        imageDataPos++;
                        imageData.data[imageDataPos] = (argb & 0X000000FF) >> 0;
                        imageDataPos++;
                        imageData.data[imageDataPos] = 255;
                        imageDataPos++;
                        sourceData++;
                }
                sourceData += dataIncAfterRow;
        }
        canvasContext.putImageData(imageData, regionX, regionY);
}
function _EMSCRIPTENQT_attemptedLocalEventLoop()
{
}
var ignoreMouseMovement = false;
var clearIgnoreMouseMovementTimerSet = false;
function EMSCRIPTENQT_mouseMoved(e)
{
	// Workaround for annoying bug in Firefox, where a flurry of mousemove events seems to starve other
	// events (timeouts, redrawing the canvas, etc).
	// Refuse to process subsequent mousemove events until a timeout has been successfully processed.
        if (ignoreMouseMovement)
        {
                if (!clearIgnoreMouseMovementTimerSet)
                {
                        setTimeout(function() { ignoreMouseMovement = false; clearIgnoreMouseMovementTimerSet = false; }, 0);
                        clearIgnoreMouseMovementTimerSet = true;
                }
                return;
        }
        var canvas = document.getElementById('canvas');
        //Module.print("Mouse moved: " + (e.pageX - canvas.offsetLeft) + "," + (e.pageY - canvas.offsetTop));
	cwrap('EMSCRIPTENQT_mouseCanvasPosChanged', 'number', ['number', 'number'])(e.pageX - canvas.offsetLeft, e.pageY - canvas.offsetTop);
        ignoreMouseMovement = true;
}
function EMSCRIPTENQT_mouseButtonEvent(e, isButtonDown)
{
        var canvas = document.getElementById('canvas');
        var qtMouseButton = -1; // See  Qt::MouseButton
        switch (e.which) {
                case 1:
                        qtMouseButton = 1;
                        break;
                case 2:
                        qtMouseButton = 4;
                        break;
                case 3:
                        qtMouseButton = 2;
                        break;
        }
        try
        {
                cwrap('EMSCRIPTENQT_mouseCanvasButtonChanged', 'number', ['number', 'number'])(qtMouseButton, (isButtonDown ? 1 : 0));
        }
        catch (e)
        {
                Module.print("Exception during mouseButton event: " + e);
        }
}
function EMSCRIPTENQT_mouseDown(e)
{
        EMSCRIPTENQT_mouseButtonEvent(e, true);
}
function EMSCRIPTENQT_mouseUp(e)
{
        EMSCRIPTENQT_mouseButtonEvent(e, false);
}
function EMSCRIPTENQT_keyEvent(e, isPress)
{
	var jsKeyCode = e.keyCode;
	var qtKeyCode = 0;
	var qtModifiers = 0;
	Module.print("keyCode: "  + jsKeyCode + " shifted: " + e.shiftKey + 'a'.charCodeAt());
	if (jsKeyCode >= 'a'.charCodeAt() && jsKeyCode <= 'z'.charCodeAt())
	{
		qtKeyCode = 0x41 + (jsKeyCode - 'a'.charCodeAt());
		Module.print("keyCode lower: "  + jsKeyCode + " shifted: " + e.shiftKey);
		e.preventDefault();
	}
	else if (jsKeyCode >= 'A'.charCodeAt() && jsKeyCode <= 'Z'.charCodeAt())
	{
		Module.print("keyCode upper: "  + jsKeyCode + " shifted: " + e.shiftKey);
		if (!e.shiftKey)
		{
			Module.print("Unshifting keycode");
			jsKeyCode -= 'A'.charCodeAt() - 'a'.charCodeAt();
			qtKeyCode = 0x41 + (jsKeyCode - 'a'.charCodeAt());
		}
		else
		{
			qtKeyCode = 0x41 + (jsKeyCode - 'A'.charCodeAt());
		}
		e.preventDefault();
	}
	else
	{
		var recognised = true;
		switch(jsKeyCode)
		{
		case 37: // Left
			qtKeyCode = 0x01000012;
			break;
		case 38: // Up
			qtKeyCode = 0x01000013;
			break;
		case 39: // Right
			qtKeyCode = 0x01000014;
			break;
		case 40: // Down
			qtKeyCode = 0x01000015;
			break;
		case 8: // Backspace
			qtKeyCode = 0x01000003;
			break;
		case 13: // Enter
			qtKeyCode = 0x01000004;
			break;
		case 32: // Enter
			qtKeyCode = 0x20;
			break;
		case 16: // Shift
                        qtKeyCode = 0x01000020;
                        break;
		default:
			recognised = false;
		};
		if (recognised)
		{
			e.preventDefault();
                        jsKeyCode = 0; // In these cases, it is incorrect to treat jsKeyCode as a unicode value.
		}
	}
	Module.print("keyCode unshifted: "  + jsKeyCode);
	// If this key event involves the shift key, and we are not *just* releasing the shift key, then add the Qt shift modifier.
        // Note that if we releasing some *non-shift* key while we are holding the shift key down, we need to add the Qt shift modifier.
        if (e.shiftKey && !(qtKeyCode == 0x01000020 && !isPress)) // i.e. don't add the shift key modifier if *all* we are doing is releasing the shift key!
        {
                qtModifiers += 0x02000000;
        }
	_EMSCRIPTENQT_canvasKeyChanged(jsKeyCode, qtKeyCode, qtModifiers, isPress, false);
}
function EMSCRIPTENQT_keyUp(e)
{
	EMSCRIPTENQT_keyEvent(e, false);
}
function EMSCRIPTENQT_keyDown(e)
{
	EMSCRIPTENQT_keyEvent(e, true);
}
function _EMSCRIPTENQT_cursorChanged(newCursorShape)
{
	//Module.print("Qt cursor changed: " + newCursorShape);
        var cssCursorStyle;

	switch(newCursorShape)
        {
        case 0: /* ArrowCursor */
		cssCursorStyle = "default";
		break;
        case 1: /* UpArrowCursor */
		cssCursorStyle = "default";
		break;
        case 2: /* CrossCursor */
		cssCursorStyle = "crosshair";
		break;
        case 3: /* WaitCursor */
		cssCursorStyle = "progress";
		break;
        case 4: /* IBeamCursor */
		cssCursorStyle = "text";
		break;
        case 5: /* SizeVerCursor */
		cssCursorStyle = "ns-resize"; 
		break;
        case 6: /* SizeHorCursor */
		cssCursorStyle = "ew-resize";
		break;
        case 7: /* SizeBDiagCursor */
		cssCursorStyle = "nesw-resize";
		break;
        case 8: /* SizeFDiagCursor */
		cssCursorStyle = "nwse-resize";
		break;
        case 9: /* SizeAllCursor */
		cssCursorStyle = "default";
		break;
        case 10: /* BlankCursor */
		cssCursorStyle = "default";
		break;
        case 11: /* SplitVCursor */
		cssCursorStyle = "row-resize";
		break;
        case 12: /* SplitHCursor */
		cssCursorStyle = "col-resize";
		break;
        case 13: /* PointingHandCursor */
		cssCursorStyle = "hand";
		break;
        case 14: /* ForbiddenCursor */
		cssCursorStyle = "not-allowed";
		break;
        case 15: /* WhatsThisCursor */
		cssCursorStyle = "help";
		break;
        case 16: /* BusyCursor */
		cssCursorStyle = "wait";
		break;
        case 17: /* OpenHandCursor */
		cssCursorStyle = "hand";
		break;
        case 18: /* ClosedHandCursor */
		cssCursorStyle = "hand";
		break;
        case 20: /* DragMoveCursor */
		cssCursorStyle = "move";
		break;
        case 21: /* DragLinkCursor */
		cssCursorStyle = "default";
		break;
	}
	var canvas = document.getElementById('canvas');
	canvas.style.cursor = cssCursorStyle;
}
Module.noExitRuntime = true;
Module['preRun'] = function() {
	try
	{
                var canvas = document.getElementById('canvas');
                canvas.addEventListener("mousemove", EMSCRIPTENQT_mouseMoved, false);
                canvas.addEventListener("mousedown", EMSCRIPTENQT_mouseDown, false);
                canvas.addEventListener("mouseup", EMSCRIPTENQT_mouseUp, false);
                canvas.addEventListener("keydown", EMSCRIPTENQT_keyDown, true);
                canvas.addEventListener("keyup", EMSCRIPTENQT_keyUp, true);
		canvas.tabIndex = 1;
		// Data cache dir for QWS
		Module['FS_createFolder']("/tmp/", 'qtembedded-0', true, true);

	} catch (e)
	{
		window.alert("Exception while setting up qws filesystem: " + e);
	}
}

