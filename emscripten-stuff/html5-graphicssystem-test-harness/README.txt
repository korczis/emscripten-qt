This the the test harness that I'll be using for the emscripten-qt HTML5 graphicssystem. The main guts is in html5-graphicssystem-test.

It will work for both emscripten and emscripten-native, but for emscripten-native, due to the fact that QtWebkit has not been ported to Emscripten (plus the fact that it would require a working graphicssystem - circular!) we must use an external, QtWebkit-based app (a standard, non-emscripten-qt app - compiled as you would a normal Qt application) and send rendering commands to that over a socket.


