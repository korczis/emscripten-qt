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

function EMSCRIPTENNATIVEHELPER_loadFont(fontDataHexString, fontFamilyHexString)
{
    try
    {
    var fontDataSize = fontDataHexString.length / 2;
    var fontFamilyStringLength = fontFamilyHexString.length / 2; // Includes null terminator.
    var totalBytes = fontDataSize + fontFamilyStringLength;
    var fakeHeap = new ArrayBuffer(totalBytes);
    var fakeHeap8 = new Uint8Array(fakeHeap);
    
    var posInFakeHeap = 0;
    var fontDataBeginInFakeHeap = 0;

    var posInFontDataHexString = 0;
    for (var i = 0; i < fontDataSize; i++)
    {
        fakeHeap8[posInFakeHeap] = parseInt(fontDataHexString.substring(posInFontDataHexString, posInFontDataHexString + 2), 16) & 0xFF;

        posInFontDataHexString += 2;
        posInFakeHeap++;
    }
    var fontFamilyStringBeginInFakeHeap = posInFakeHeap;
    

    var posInFontFamilyHexString = 0;
    for (var i = 0; i < fontFamilyStringLength; i++)
    {
        fakeHeap8[posInFakeHeap] = parseInt(fontFamilyHexString.substring(posInFontFamilyHexString, posInFontFamilyHexString + 2), 16) & 0xFF;

        posInFontFamilyHexString += 2;
        posInFakeHeap++;
    }

    _EMSCRIPTENQT_loadFont_internal(fontDataBeginInFakeHeap, fontDataSize, fontFamilyStringBeginInFakeHeap, fakeHeap8);
    }
    catch (e)
    {
        window.alert("Exception in EMSCRIPTENNATIVEHELPER_loadFont: " + e);
    }
}
function Pointer_stringify_nativehelper(ptr, HEAPU8) {
  // Find the length, and check for UTF while doing so
  var hasUtf = false;
  var t;
  var i = 0;
  var length = 0;
  while (1) {
    t = HEAPU8[(((ptr)+(i))|0)];
    if (t >= 128) hasUtf = true;
    else if (t == 0 && !length) break;
    i++;
    if (length && i == length) break;
  }
  if (!length) length = i;
  var ret = '';
  if (!hasUtf) {
    var MAX_CHUNK = 1024; // split up into chunks, because .apply on a huge string can overflow the stack
    var curr;
    while (length > 0) {
      var dataArray = [];
      for (var j = 0, jj = length; j < jj; ++j) {
          dataArray.push(HEAPU8[ptr + j]);
      }
      curr = String.fromCharCode.apply(null, dataArray);
      //curr = String.fromCharCode.apply(String, HEAPU8.subarray(ptr, ptr + Math.min(length, MAX_CHUNK))); See https://github.com/mozilla/pdf.js/issues/1820
      ret = ret ? ret + curr : curr;
      ptr += MAX_CHUNK;
      length -= MAX_CHUNK;
    }
    return ret;
  }
  var utf8 = new Runtime.UTF8Processor();
  for (i = 0; i < length; i++) {
    t = HEAPU8[(((ptr)+(i))|0)];
    ret += utf8.processCChar(t);
  }
  return ret;
}
