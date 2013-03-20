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
    var canvas = EMSCRIPTENNATIVEHELPER_getCanvas();
    var numPixels = canvas.width  * canvas.height;
    var pixels = canvas.getContext("2d").getImageData(0, 0, canvas.width, canvas.height);
    
    var pixelsAsHexArray = new Array(numPixels);
    for (var pixelNum = 0; pixelNum < numPixels * 4; pixelNum++)
    {
        var value = pixels.data[pixelNum];
        var hex = value.toString(16);
        if (hex.length < 2)
        {
            hex = "0" + hex;
        }
        pixelsAsHexArray[pixelNum] = hex;
    }
    return pixelsAsHexArray.join("");
}
