try
{
        if (Float64Array == null)
        {
                throw "";
        }
}
catch (e)
{
        Module.setStatus("Aborted - Unsupported Browser :/");
        window.alert( "Javascript typed arrays apparently are not supported by your browser: unfortunately, emscripten-qt apps cannot run in it.");
        throw e;
}

var EMSCRIPTENQT_callbackTimer = null;
var EMSCRIPTENQT_mainLoopHasBeenInitialised = false;
function _EMSCRIPTENQT_mainLoopInitialised_internal()
{
    EMSCRIPTENQT_mainLoopHasBeenInitialised = true;
}
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
function _EMSCRIPTENQT_resetTimerCallback_internal(milliseconds)
{
	if (EMSCRIPTENQT_callbackTimer != null)
	{
		clearTimeout(EMSCRIPTENQT_callbackTimer);
		EMSCRIPTENQT_callbackTimer = null;
	}
	EMSCRIPTENQT_callbackTimer = setTimeout(_EMSCRIPTENQT_timerCallback_springboard, milliseconds);
	//Module.print("callback requested in " + milliseconds + "ms");
}
function _EMSCRIPTENQT_canvas_width_pixels_internal()
{
	var canvas = document.getElementById('canvas');
	return canvas.width;
}
function _EMSCRIPTENQT_canvas_height_pixels_internal()
{
	var canvas = document.getElementById('canvas');
	return canvas.height;
}
var lastRenderedData = null;
function _EMSCRIPTENQT_flush_pixels_typed_array(data, regionX, regionY, regionW, regionH)
{
	// Experimental canvas rendered that makes use of typed arrays.
        var canvasWidth = _EMSCRIPTENQT_canvas_width_pixels();
        var canvasHeight = _EMSCRIPTENQT_canvas_height_pixels();
        var fullRepaint = (lastRenderedData == null);
	if (lastRenderedData == null)
	{
		var totalPixels = canvasWidth * canvasHeight;
		lastRenderedData = new Int32Array(totalPixels); 
		for (var i = 0; i < totalPixels; i++)
		{
			lastRenderedData[i] = 0xFFFFFFFF;
		}
	}
        var canvas = document.getElementById('canvas');
        var canvasContext = canvas.getContext("2d");
        var imageDataRaw = canvasContext.getImageData(regionX, regionY, regionW, regionH);
        var imageDataPos = 0;
        var dataIncAfterRow = (canvasWidth - regionW);
        var sourceData = (data >> 2) + canvasWidth * regionY + regionX;
	var lastRenderedDataPos = canvasWidth * regionY + regionX;
	var imageDataRawBuffer = new ArrayBuffer(imageDataRaw.data.length);
	var imageData = new Int32Array(imageDataRawBuffer);
	var imageData8 = new Uint8ClampedArray(imageDataRawBuffer);
	imageData8.set(imageDataRaw.data);
        for (var y = 0; y < regionH; y++)
        {
                for (var x = 0; x < regionW; x++)
                {
                        var argb = HEAP32[sourceData];
			if (argb != lastRenderedData[lastRenderedDataPos] || fullRepaint)
			{
				argb = argb & 0x00FFFFFF;
				var convertedToCanvasPixel = (0xFF << 24) |
                                   (((argb & 0x0000FF) >> 0) << 16) |
                                   (((argb & 0x00FF00) >> 8) << 8) |
                                   (((argb & 0xFF0000) >> 16) << 0) ;
				imageData[imageDataPos] = convertedToCanvasPixel; 
			}
                        imageDataPos++;
                        sourceData++;
			lastRenderedDataPos++;
                }
                sourceData += dataIncAfterRow;
                lastRenderedDataPos += dataIncAfterRow;
        }
	imageDataRaw.data.set(imageData8);
        canvasContext.putImageData(imageDataRaw, regionX, regionY);
	lastRenderedData.set(HEAP32.subarray(data >> 2, (data >> 2) + (canvasWidth * canvasHeight)));
}
function _EMSCRIPTENQT_flush_pixels_normal(data, regionX, regionY, regionW, regionH)
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
function _EMSCRIPTENQT_flush_pixels_internal(data, regionX, regionY, regionW, regionH)
{
        if (document.getElementById('experimental-renderer-checkbox').checked)
        {
                _EMSCRIPTENQT_flush_pixels_typed_array(data, regionX, regionY, regionW, regionH);
        }
        else
        {
                _EMSCRIPTENQT_flush_pixels_normal(data, regionX, regionY, regionW, regionH);
        }
}
function _EMSCRIPTENQT_attemptedLocalEventLoop()
{
}
function _EMSCRIPTENQT_launchWebBrowser_internal(urlCString)
{
    window.open(Pointer_stringify(urlCString), '_blank');
}
var ignoreMouseMovement = false;
var clearIgnoreMouseMovementTimerSet = false;
function EMSCRIPTENQT_mouseMoved(e)
{
    if (!EMSCRIPTENQT_mainLoopHasBeenInitialised)
    {
        return;
    }
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
    if (!EMSCRIPTENQT_mainLoopHasBeenInitialised)
    {
        return;
    }
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
var lastKeyPressCharCode = null;
var lastKeyDownCharCode = null;
var waitingForkeyPress = false;
function EMSCRIPTENQT_keyEvent(e, isKeyDown, isKeyPress)
{
    if (!EMSCRIPTENQT_mainLoopHasBeenInitialised)
    {
        return;
    }
    var wasWaitingForKeyPress = waitingForkeyPress;
    waitingForkeyPress = false;
    if (e.ctrlKey )
    {
        e.preventDefault();
    }
	var jsKeyCode = e.keyCode;
	var qtKeyCode = 0;
	var qtModifiers = 0;

	var jsKeyCodeIsValidUnicode = true;
	var recognised = true;
    if (!(isKeyPress && lastKeyDownCharCode != jsKeyCode)) // Sigh - Chrome's "(" is almost indistinguishable from
        // its shift+down, save that the keypress and keydown keyCodes are different in the  former.
    {
        // keycodes  that we can immediately recognise - generally ones whose meaning
        // does not change when coupled with a shift.
        switch(jsKeyCode)
        {
            case 37: // Left
                qtKeyCode = 0x01000012;
                jsKeyCodeIsValidUnicode = false;
                break;
            case 38: // Up
                qtKeyCode = 0x01000013;
                jsKeyCodeIsValidUnicode = false;
                break;
            case 39: // Right
                qtKeyCode = 0x01000014;
                jsKeyCodeIsValidUnicode = false;
                break;
            case 40: // Down
                qtKeyCode = 0x01000015;
                jsKeyCodeIsValidUnicode = false;
                break;
            case 8: // Backspace
                qtKeyCode = 0x01000003;
                break;
            case 13: // Enter
                qtKeyCode = 0x01000004;
                break;
            case 27: // Esc
                qtKeyCode = 0x01000000;
                break;
            case 32: // Space
                qtKeyCode = 0x20;
                break;
            case 16: // Shift
                qtKeyCode = 0x01000020;
                break;
            case 17: // Ctrl
                qtKeyCode = 0x01000021;
                break;
            default:
                recognised = false;
        };
    }
    else 
    {
        recognised = false;
    }
	if (recognised)
	{
		e.preventDefault();
		if (isKeyPress)
		{
			// We already processed this in the corresponding keydown, thanks!
			return;
		}
	}
	else if (isKeyDown && !isKeyPress)
	{
		// Need to wait for the pending keypress in order to identify the key.
        lastKeyDownCharCode = jsKeyCode;
        waitingForkeyPress = true;;
		return;
	}

	if (isKeyPress)
	{
		// If we're here, then we're figuring out the code for the last keypress.
		if (isKeyDown)
		{
			jsKeyCode = e.which;
			// Store this so that we can properly handle a key up for this key.
			lastKeyPressCharCode = jsKeyCode;
			if (jsKeyCode == 32) 
                        { 
                                // Hack around strange behaviour in Firefox, which seems to deliver a keypress 
                                // event for spaces, but with e.keyCode = 0 
                                return; 
                        }

		}
		else
		{
			jsKeyCode = lastKeyPressCharCode;
		}
		e.preventDefault();
	}
	if (!jsKeyCodeIsValidUnicode)
	{
		jsKeyCode = 0; 
	}
	// If this key event involves the shift key, and we are not *just* releasing the shift key, then add the Qt shift modifier.
	// Note that if we releasing some *non-shift* key while we are holding the shift key down, we need to add the Qt shift modifier.
	if (e.shiftKey && !(qtKeyCode == 0x01000020 && !isKeyDown)) 
	{
		qtModifiers += 0x02000000;
	}
	// The same as shift, but for ctrl.
	if (e.ctrlKey && !(qtKeyCode == 0x01000021 && !isKeyDown) ) 
	{
		qtModifiers += 0x04000000;
	}
    if (e.ctrlKey && (jsKeyCode >= "A".charCodeAt(0) && jsKeyCode <= "Z".charCodeAt(0)))
    {
        jsKeyCode = "a".charCodeAt(0) + (jsKeyCode - "A".charCodeAt(0));
    }
    if (wasWaitingForKeyPress && !isKeyPress)
    {
        // Damn you, Chrome: synthesise the keyPress event that we should have had
        _EMSCRIPTENQT_canvasKeyChanged(jsKeyCode, qtKeyCode, qtModifiers, true, false);
    }
	_EMSCRIPTENQT_canvasKeyChanged(jsKeyCode, qtKeyCode, qtModifiers, isKeyDown, false);
}
function EMSCRIPTENQT_keyUp(e)
{
	EMSCRIPTENQT_keyEvent(e, false, false);
}
function EMSCRIPTENQT_keyDown(e)
{
	EMSCRIPTENQT_keyEvent(e, true, false);
}
function EMSCRIPTENQT_keyPress(e)
{
	EMSCRIPTENQT_keyEvent(e, true, true);
}
function _EMSCRIPTENQT_cursorChanged_internal(newCursorShape)
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

function addDownloadableFile(filePath, linkTextWhenFileDoesNotExist, linkTextWhenFileDoesExist, fileMimeType)
{
    // Add a link that we can click with instructions to save your file.
    var linkUrlParagraph = document.createElement("p");
    canvas.parentNode.insertBefore(linkUrlParagraph, canvas.nextSibling);
    linkUrlParagraph.style.textAlign = "center";
    var linkUrl = document.createElement("a");
    linkUrl.href="javascript: void(0)";
    linkUrl.innerHTML = linkTextWhenFileDoesNotExist;
    linkUrl.target = "_blank";
    linkUrlParagraph.appendChild(linkUrl);
    var fileTimeStamp = null;
    setInterval(function() {
            var file = FS.analyzePath(filePath).object;
            if (fileTimeStamp != null && file["timestamp"] == fileTimeStamp)
            {
                return;
            }
            var contents;
            try
            {
                contents = file["contents"];
            }
            catch (e)
            {
                return;
            }
            function encode64(data) {
                var BASE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
                var PAD = '=';
                var ret = '';
                var leftchar = 0;
                var leftbits = 0;
                for (var i = 0; i < data.length; i++) {
                    leftchar = (leftchar << 8) | data[i];
                    leftbits += 8;
                    while (leftbits >= 6) {
                        var curr = (leftchar >> (leftbits-6)) & 0x3f;
                        leftbits -= 6;
                        ret += BASE[curr];
                    }
                }
                if (leftbits == 2) {
                    ret += BASE[(leftchar&3) << 4];
                    ret += PAD + PAD;
                } else if (leftbits == 4) {
                    ret += BASE[(leftchar&0xf) << 2];
                    ret += PAD;
                }
                    return ret;
            };
            linkUrl.href = "data:" + fileMimeType +  ";base64," + encode64(contents);
            linkUrl.innerHTML = linkTextWhenFileDoesExist;
            fileTimeStamp = file["timestamp"];
    }, 1000);
}

Module.noExitRuntime = true;
Module['preRun'].push(function() {
	try
	{
                var canvas = document.getElementById('canvas');
                canvas.onfocus = function()
                {
                    // Capturing at the document level is necessary in Chrome if we want to intercept e.g.
                    // ctrl+c, etc.
                    document.addEventListener("mousemove", EMSCRIPTENQT_mouseMoved, false);
                    document.addEventListener("mousedown", EMSCRIPTENQT_mouseDown, false);
                    document.addEventListener("mouseup", EMSCRIPTENQT_mouseUp, false);
                    document.addEventListener("keydown", EMSCRIPTENQT_keyDown, true);
                    document.addEventListener("keyup", EMSCRIPTENQT_keyUp, true);
                    document.addEventListener("keypress", EMSCRIPTENQT_keyPress, true);
                }
                canvas.onblur = function()
                {
                    // Capturing at the document level is necessary in Chrome if we want to intercept e.g.
                    // ctrl+c, etc.
                    document.removeEventListener("mousemove", EMSCRIPTENQT_mouseMoved, false);
                    document.removeEventListener("mousedown", EMSCRIPTENQT_mouseDown, false);
                    document.removeEventListener("mouseup", EMSCRIPTENQT_mouseUp, false);
                    document.removeEventListener("keydown", EMSCRIPTENQT_keyDown, true);
                    document.removeEventListener("keyup", EMSCRIPTENQT_keyUp, true);
                    document.removeEventListener("keypress", EMSCRIPTENQT_keyPress, true);
                }
		// Add the 'use experimental renderer' checkbox beneath the canvas.
                var experimentalRendererCheckbox = document.createElement("input");
                experimentalRendererCheckbox.id = "experimental-renderer-checkbox";
                experimentalRendererCheckbox.type = "checkbox";
                experimentalRendererCheckbox.checked = false;
                // Trigger a full redraw if experimental renderer is switched on.
                experimentalRendererCheckbox.onchange = function() { if (experimentalRendererCheckbox.checked) { lastRenderedData = null;}; } ;
                var paragraphForExperimentalCheckboxRenderer = document.createElement("p");
                paragraphForExperimentalCheckboxRenderer.appendChild(experimentalRendererCheckbox);
                paragraphForExperimentalCheckboxRenderer.style.textAlign = "center";
                canvas.parentNode.insertBefore(paragraphForExperimentalCheckboxRenderer, canvas.nextSibling);
                var experimentalRendererLabel = document.createElement('label');
                experimentalRendererLabel.innerHTML = "Use experimental renderer";
                paragraphForExperimentalCheckboxRenderer.appendChild(experimentalRendererLabel);


		canvas.tabIndex = 1;
		// Data cache dir for QWS
		Module['FS_createFolder']("/", 'tmp', true, true);
		Module['FS_createFolder']("/tmp/", 'qtembedded-0', true, true);

        // Decode the arguments provided by any "?args='blah blah'" in the URL.

        // parseUri 1.2.2
        // (c) Steven Levithan <stevenlevithan.com>
        // MIT License

        function parseUri (str) {
            var o   = parseUri.options,
                m   = o.parser[o.strictMode ? "strict" : "loose"].exec(str),
                uri = {},
                i   = 14;

            while (i--) uri[o.key[i]] = m[i] || "";

            uri[o.q.name] = {};
            uri[o.key[12]].replace(o.q.parser, function ($0, $1, $2) {
                if ($1) uri[o.q.name][$1] = $2;
            });

            return uri;
        };

        parseUri.options = {
            strictMode: false,
            key: ["source","protocol","authority","userInfo","user","password","host","port","relative","path","directory","file","query","anchor"],
            q:   {
                name:   "queryKey",
                parser: /(?:^|&)([^&=]*)=?([^&]*)/g
            },
            parser: {
                strict: /^(?:([^:\/?#]+):)?(?:\/\/((?:(([^:@]*)(?::([^:@]*))?)?@)?([^:\/?#]*)(?::(\d*))?))?((((?:[^?#\/]*\/)*)([^?#]*))(?:\?([^#]*))?(?:#(.*))?)/,
                loose:  /^(?:(?![^:@]+:[^:@\/]*@)([^:\/?#.]+):)?(?:\/\/)?((?:(([^:@]*)(?::([^:@]*))?)?@)?([^:\/?#]*)(?::(\d*))?)(((\/(?:[^?#](?![^?#\/]*\.[^?#\/.]+(?:[?#]|$)))*\/?)?([^?#\/]*))(?:\?([^#]*))?(?:#(.*))?)/
            }
        };
        var argsRaw = parseUri(decodeURI(window.location.href)).queryKey.args;
        if (argsRaw)
        {
            Module['arguments'] = argsRaw.split(" ");
        }
	} catch (e)
	{
		window.alert("Exception while setting up qws filesystem: " + e);
	}
});

function _EMSCRIPTENQT_handleForMainCanvas_internal()
{
try
{
    if (document.getElementById('canvas') == undefined)
    {
        return -1;
    }
    else
    {
       return 0;
    }
}
catch (e)
{
window.alert("Exception: " + e);
}
}

function _EMSCRIPTENQT_mainCanvasWidth_internal()
{
    var mainCanvas = document.getElementById('canvas');
    if (mainCanvas == undefined)
    {
        return -1;
    }
    else
    {
       return mainCanvas.width;
    }
}

function _EMSCRIPTENQT_mainCanvasHeight_internal()
{
    var mainCanvas = document.getElementById('canvas');
    if (mainCanvas == undefined)
    {
        return -1;
    }
    else
    {
       return mainCanvas.height;
    }
}

var emscriptenqt_handle_to_canvas;
function _EMSCRIPTENQT_createCanvas_internal(width, height)
{
    try
    {
        // handle 0 is always reserved for the main canvas.
        var handle = 1;
        if (!emscriptenqt_handle_to_canvas)
        {
            emscriptenqt_handle_to_canvas = new Object;
        }
        while (emscriptenqt_handle_to_canvas[handle] != null)
        {
            handle = handle + 1;
        }
        var canvas = document.createElement("canvas");      
        canvas.width = width;
        canvas.height = height; 
        canvas.savedStateCount = 0;
        canvas.savedClips = [];
        emscriptenqt_handle_to_canvas[handle] = canvas;
        var ctx = canvas.getContext("2d");
        canvas.trackedRestore = function() { canvas.savedStateCount--; ctx.restore();}
        canvas.trackedSave = function() {canvas.savedStateCount++; ctx.save(); }
        canvas.trackedSave(); // Store the initial state. 
        //window.alert("default strokeStyle: " + ctx.strokeStyle);
        
        return handle;
    }
    catch (e)
    {
        Module.print("Creating canvas of size: " + width + "," + height + " failed: " + e);
        return -1;
    }
}

function _EMSCRIPTENQT_fillSolidRect_internal(canvasHandle, r, g, b, x, y, width, height)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    ctx.beginPath();
    ctx.rect(x, y, width, height);
    ctx.fillStyle = "rgba(" + r + "," + g + "," + b + "," + 0xff + ")";
    ctx.fill();
    ctx.closePath();
    return true;
}

function _EMSCRIPTENQT_strokeRect_internal(canvasHandle, x, y, width, height)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    ctx.strokeRect(x + 0.5, y + 0.5,  width, height);
}

function _EMSCRIPTENQT_fillRect_internal(canvasHandle, x, y, width, height)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    ctx.fillRect(x + 0.5, y + 0.5,  width, height);
}

function _EMSCRIPTENQT_strokeEllipse_internal(canvasHandle, cx, cy, width, height)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    var originalLineWidth = ctx.lineWidth;

    // There is, astoundingly, no well-supported ellipse drawing functionality in HTML5 yet,
    // so we must fake it by drawing a stretched circle.
    // Of course, this doesn't really work too well, since the line will be stretched by possibly different
    // amounts in the horizontal and vertical dimensions, leading to a lack of uniform thickness :/
    ctx.save(); 
    ctx.beginPath();

    ctx.translate(cx - width / 2, cy - height / 2);
    ctx.scale(width / 2, height / 2);
    // Ugh - there's always going to be some distortion of the line, but let's try and minimise it - although
    // this really doesn't do any good :/
    if (width < height)
    {
        ctx.lineWidth = 2 * originalLineWidth / width;
    }
    else
    {
        ctx.lineWidth = 2 * originalLineWidth / height;
    }
    ctx.arc(1, 1, 1, 0, 2 * Math.PI, false);

    ctx.stroke();
    ctx.restore(); 
}

function _EMSCRIPTENQT_fillEllipse_internal(canvasHandle, cx, cy, width, height)
{
    // See _EMSCRIPTENQT_strokeEllipse_internal.
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    var originalLineWidth = ctx.lineWidth;

    ctx.save(); 
    ctx.beginPath();

    ctx.translate(cx - width / 2, cy - height / 2);
    ctx.scale(width / 2, height / 2);
    ctx.arc(1, 1, 1, 0, 2 * Math.PI, false);

    ctx.fill();
    ctx.restore(); 
}

function _EMSCRIPTENQT_drawLine_internal(canvasHandle, startX, startY, endX, endY)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");

    ctx.beginPath();
    ctx.moveTo(startX + 0.5, startY + 0.5);
    ctx.lineTo(endX + 0.5, endY + 0.5);
    ctx.closePath();
    ctx.stroke();
    
}

function _EMSCRIPTENQT_changePenColor_internal(canvasHandle, r, g, b)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    ctx.strokeStyle = "rgba(" + r + "," + g + "," + b + "," + 0xFF + ")";
}

function _EMSCRIPTENQT_changePenThickness_internal(canvasHandle, thickness)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    ctx.lineWidth = thickness;
}

var _EMSCRIPTENQT_currentGradient = null;
function _EMSCRIPTENQT_createLinearGradient_internal(canvasHandle, startX, startY, endX, endY)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    _EMSCRIPTENQT_currentGradient = ctx.createLinearGradient(startX, startY, endX, endY);
}

function _EMSCRIPTENQT_addStopPointToCurrentGradient_internal(position, r, g, b)
{
    _EMSCRIPTENQT_currentGradient.addColorStop(position, "rgba(" + r + "," + g + "," + b + "," + 0xFF + ")");
}

function _EMSCRIPTENQT_setBrushToCurrentGradient_internal(canvasHandle)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    ctx.fillStyle = _EMSCRIPTENQT_currentGradient;
}

function _EMSCRIPTENQT_changeBrushColor_internal(canvasHandle, r, g, b)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    ctx.fillStyle = "rgba(" + r + "," + g + "," + b + "," + 0xFF + ")";
}

function _EMSCRIPTENQT_changeBrushTexture_internal(canvasHandle, textureCanvasHandle)
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var textureCanvas = emscriptenqt_handle_to_canvas[textureCanvasHandle];
    var ctx = canvas.getContext("2d");
    var texturePattern = ctx.createPattern(textureCanvas, "repeat");

    ctx.fillStyle = texturePattern;
}

function _EMSCRIPTENQT_removeClip_internal(canvasCtx, canvas)
{
    // Assumes that canvas has a clip.  Results are undefined otherwise!
    var oldFillStyle = canvasCtx.fillStyle;
    var oldLineWidth = canvasCtx.lineWidth;
    var oldStrokeStyle = canvasCtx.strokeStyle;
    canvas.trackedRestore();
    canvas.hasClip = false;
    canvasCtx.fillStyle = oldFillStyle;
    canvasCtx.lineWidth = oldLineWidth;
    canvasCtx.strokeStyle = oldStrokeStyle;
}

function _EMSCRIPTENQT_savePaintState_internal(canvasHandle)
{
try
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    if (canvas.hasClip)
    {
        canvas.savedClips.push(canvas.lastSetClip);
        _EMSCRIPTENQT_removeClip_internal(ctx, canvas);
    }
    else
    {
        canvas.savedClips.push(null);
    }
    canvas.trackedSave();
    if (canvas.lastSetClip)
    {
        // TODO - need to be able to save and restore more complex paths, too (i.e. resulting from a QPainterPainter instead of a rectangle).
        _EMSCRIPTENQT_setClipRect_internal(canvasHandle, canvas.lastSetClip[0], canvas.lastSetClip[1], canvas.lastSetClip[2], canvas.lastSetClip[3]);
    }
}
catch(e)
{
window.alert("_EMSCRIPTENQT_savePaintState_internal: " + canvasHandle + "  e: " + e);
}
}

function _EMSCRIPTENQT_restorePaintState_internal(canvasHandle)
{
try
{
    //window.alert("restore");
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    if (canvas.hasClip)
    {
        _EMSCRIPTENQT_removeClip_internal(ctx, canvas);
    }
    canvas.trackedRestore();
    var clipToRestore = canvas.savedClips.pop();
    if (clipToRestore)
    {
        // TODO - need to be able to save and restore more complex paths, too (i.e. resulting from a QPainterPainter instead of a rectangle).
        _EMSCRIPTENQT_setClipRect_internal(canvasHandle, clipToRestore[0], clipToRestore[1], clipToRestore[2], clipToRestore[3]);
    }
}
catch(e)
{
window.alert("_EMSCRIPTENQT_restorePaintState_internal e: " + e);
}
}

function _EMSCRIPTENQT_restoreToOriginalState_internal(canvasHandle)
{
try
{
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    while (canvas.savedStateCount > 0) 
    {
        canvas.trackedRestore();
    }
    canvas.trackedSave();
    canvas.hasClip = false;
}
catch(e)
{
    window.alert("restoreToOriginal e: " + e);
}
}


function _EMSCRIPTENQT_setClipRect_internal(canvasHandle, x, y, width, height)
{
    try
    {
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    if (canvas.hasClip)
    {
        _EMSCRIPTENQT_removeClip_internal(ctx, canvas);
    }
    canvas.trackedSave();
    ctx.beginPath();
    ctx.rect(x , y,  width, height);
    ctx.clip();
    ctx.closePath();
    canvas.hasClip = true;
    canvas.lastSetClip = [x, y, width, height]; 
    }
    catch (e)
    {
        window.alert("e: " + e);
    }
}

function _EMSCRIPTENQT_setTransform_internal(canvasHandle, a, b, c, d, e, f)
{
    try
    {
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    ctx.setTransform(a, b, c, d, e, f);
    }
    catch (e)
    {
        window.alert("e: " + e);
    }
}
function _EMSCRIPTENQT_setCanvasPixelsRaw_internal(canvasHandle, sourcePointer, heapArray8)
{
try
{
    // TODO - optimise - use a low-level copy, or something.
    if (heapArray8 == undefined)
    {
        heapArray8 = HEAP8;
    }
    var canvas = emscriptenqt_handle_to_canvas[canvasHandle];
    var ctx = canvas.getContext("2d");
    var canvasData = ctx.createImageData(canvas.width, canvas.height);
    var canvasDataRaw = canvasData.data;
    var numComponents = canvas.width * canvas.height * 4;
    for (var i = 0; i < numComponents; i++)
    {
        canvasDataRaw[i] = heapArray8[sourcePointer] & 0xFF;
        sourcePointer++;
    } 
    ctx.putImageData(canvasData, 0, 0);
}
catch (e)
{
    window.alert("setCanvasPixelsRaw e: " + e);
}
}
function _EMSCRIPTENQT_drawCanvasOnCanvas_internal(canvasHandleToDraw, canvasHandleToDrawOn, x, y)
{
try
{
    var canvasToDraw = emscriptenqt_handle_to_canvas[canvasHandleToDraw];
    var canvasToDrawOn = emscriptenqt_handle_to_canvas[canvasHandleToDrawOn];
    var contextToDrawOn = canvasToDrawOn.getContext("2d");
    contextToDrawOn.drawImage(canvasToDraw, x, y);
    return true;
}
catch (e)
{
window.alert("drawCanvasOnCanvas e: " + e);
}
}

function _EMSCRIPTENQT_drawStretchedCanvasPortionOnCanvas_internal(canvasHandleToDraw, canvasHandleToDrawOn, targetX, targetY, targetWidth, targetHeight, sourceX, sourceY, sourceWidth, sourceHeight)
{
try
{
    var canvasToDraw = emscriptenqt_handle_to_canvas[canvasHandleToDraw];
    var canvasToDrawOn = emscriptenqt_handle_to_canvas[canvasHandleToDrawOn];
    var contextToDrawOn = canvasToDrawOn.getContext("2d");
    contextToDrawOn.drawImage(canvasToDraw, sourceX, sourceY, sourceWidth, sourceHeight, targetX, targetY, targetWidth, targetHeight);
    return true;
}
catch (e)
{
window.alert("drawImagePortion e: " + e);
}
}

function _EMSCRIPTENQT_drawCanvasOnMainCanvas_internal(canvasHandle, x, y)
{
try
{
    var canvasToDraw = emscriptenqt_handle_to_canvas[canvasHandle];
    var mainCanvas = document.getElementById('canvas');
    var mainCanvasCtx = canvas.getContext("2d");
    mainCanvasCtx.drawImage(canvasToDraw, x, y);
    return true;
}
catch (e)
{
window.alert("drawCanvasOnMainCanvas (" + canvasHandle + "," + x + "," + y + ") e: " + e);
}
}

function _EMSCRIPTENQT_mainCanvasContentsRaw_internal(destPointer, heapArray8)
{
    try
    {
        if (heapArray8 == undefined)
        {
            heapArray8 = HEAP8;
        }
        var destHeapArrayIndex = destPointer;
        var canvas = document.getElementById('canvas');
        var numPixels = canvas.width  * canvas.height;
        var pixelData = canvas.getContext("2d").getImageData(0, 0, canvas.width, canvas.height).data;
        var componentIndex = 0;
        for (var componentIndex = 0; componentIndex < 4 * numPixels; componentIndex++)
        {
            var componentValue = pixelData[componentIndex];
            heapArray8[destHeapArrayIndex] = componentValue;
            //window.alert("heapArray32: " + destHeapArrayIndex + " = " + heapArray8[destHeapArrayIndex]);
            destHeapArrayIndex++;
        }
        return true;
    }
    catch (e)
    {
        window.alert("Exception (_EMSCRIPTENQT_mainCanvasContentsRaw_internal): " + e);
    }
}

function _EMSCRIPTENQT_clearMainCanvas_internal(rgba)
{
    try 
    { 
    var canvas = document.getElementById('canvas');
    var numPixels = canvas.width  * canvas.height; 
    var pixels = canvas.getContext("2d").getImageData(0, 0, canvas.width, canvas.height); 
    for (var pixelNum = 0; pixelNum < numPixels * 4; ) 
    { 
        pixels.data[pixelNum] = ((rgba & 0xFF000000) >> 24) & 0xFF; 
        pixelNum++; 
        pixels.data[pixelNum] = ((rgba & 0x00FF0000) >> 16) & 0xFF; 
        pixelNum++; 
        pixels.data[pixelNum] = ((rgba & 0x0000FF00) >> 8) & 0xFF; 
        pixelNum++; 
        pixels.data[pixelNum] = ((rgba & 0x000000FF) >> 0) & 0xFF; 
        pixelNum++; 
    } 
    canvas.getContext("2d").putImageData(pixels, 0, 0); 
    } 
    catch (e) 
    { 
        return e; 
    } 
    return "OK"; 
}

var numFramesToSample = 10;
var lastSampledFramesFinishTimes = [];
var fpsCounter = document.getElementById('fpscounter');
function _EMSCRIPTENQT_notify_frame_rendered_internal()
{
    if (!fpsCounter)
    {
        return;
    }
    var now = Date.now();
    if (lastSampledFramesFinishTimes.length == numFramesToSample)
    {
        // Remove first (i.e. oldest) element.
        lastSampledFramesFinishTimes.splice(0,1);
    }
    lastSampledFramesFinishTimes.push(now);
    if (lastSampledFramesFinishTimes.length == numFramesToSample)
    {
        var millisecondsForLastSetOfSampledFrames = now - lastSampledFramesFinishTimes[0];
        var millisPerFrame = millisecondsForLastSetOfSampledFrames / (numFramesToSample - 1);
        var framesPerSecond = 1000 / millisPerFrame;
        fpsCounter.innerHTML = "FPS: " + framesPerSecond;
    }
}
