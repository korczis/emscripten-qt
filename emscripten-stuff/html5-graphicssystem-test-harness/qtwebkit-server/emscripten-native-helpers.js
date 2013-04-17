function EMSCRIPTENNATIVEHELPER_getCanvas()
{
    return document.getElementById('canvas');
}
function EMSCRIPTENNATIVEHELPER_clearCanvas(rgba)
{
    try
    {
    var canvas = EMSCRIPTENNATIVEHELPER_getCanvas();
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
function EMSCRIPTENNATIVEHELPER_canvasPixelsAsRGBAString()
{
try
{
    var canvas = EMSCRIPTENNATIVEHELPER_getCanvas();
    var numPixels = canvas.width  * canvas.height;

    var fakeHeap = new ArrayBuffer(numPixels * 4);
    var fakeHeap8 = new Int8Array(fakeHeap);
    var destIndexInFakeHeap = 0;


    _EMSCRIPTENQT_mainCanvasContentsRaw_internal(destIndexInFakeHeap, fakeHeap8);

    var pixelsAsHexArray = new Array(numPixels);
    for (var rgbaComponentIndex = 0; rgbaComponentIndex < numPixels * 4; rgbaComponentIndex++)
    {
        var componentValue = fakeHeap8[rgbaComponentIndex] & 0xFF;
        var hex = componentValue.toString(16);
        if (hex.length < 2)
        {
            hex = "0" + hex;
        }
        pixelsAsHexArray[rgbaComponentIndex] = hex;
    }
    return pixelsAsHexArray.join("");
}
catch(e)
{
window.alert("Exception: " + e);
}
}

function EMSCRIPTENNATIVEHELPER_setCanvasPixelsRaw(canvasHandle, rgbaHexString, width, height)
{
    // Construct a byte-array containing the equivalent of the uchar* rgba data.
    var numPixels = width * height;
    var fakeHeap = new ArrayBuffer(numPixels * 4);
    var fakeHeap8 = new Int8Array(fakeHeap);
    var posInRgbaHexString = 0;
    for (var i = 0; i < numPixels * 4; i++)
    {
        var asDec = parseInt(rgbaHexString.substring(posInRgbaHexString, posInRgbaHexString + 2), 16) & 0xFF;
        fakeHeap8[i] = asDec;
        
        posInRgbaHexString += 2;
    }
    _EMSCRIPTENQT_setCanvasPixelsRaw_internal(canvasHandle, 0, fakeHeap8);
    
}
