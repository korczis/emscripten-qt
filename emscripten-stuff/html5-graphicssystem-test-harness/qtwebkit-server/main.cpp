#include <QtGui/QApplication>
#include <QtWebKit/QWebView>
#include <QtCore/QFile>
#include <QtCore/QDebug>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        qDebug() << "Expected: html-filename javascript-filename\n\n\tThe html in html-filename should contain the token %1, which will be replaced with the contents of javascript-filename.  It should also have a canvas element with id 'canvas' and the dimensions specified in canvasdimensions.h";
        return EXIT_FAILURE;
    }

    const QString htmlFilename(argv[1]);
    QFile htmlShellFile(htmlFilename);
    if (!htmlShellFile.open(QIODevice::ReadOnly))
    {
        qDebug() << "Could not find find " << htmlFilename;
        return EXIT_FAILURE;
    }
    const QString originalHtml = htmlShellFile.readAll();
    if (!originalHtml.contains("%1"))
    {
        qDebug() << "The file " << htmlFilename << " does not contain the '%1' token!";
        return EXIT_FAILURE;
    }

    const QString javascriptFilename(argv[2]);
    QFile javascriptShellFile(javascriptFilename);
    if (!javascriptShellFile.open(QIODevice::ReadOnly))
    {
        qDebug() << "Could not find find " << javascriptFilename;
        return EXIT_FAILURE;
    }
    const QString javascript = javascriptShellFile.readAll();

    const QString emscriptenNativeHelpersFilename("emscripten-native-helpers.js");
    QFile emscriptenNativeHelpersFile(emscriptenNativeHelpersFilename);
    if (!emscriptenNativeHelpersFile.open(QIODevice::ReadOnly))
    {
        qDebug() << "Could not find find " << emscriptenNativeHelpersFilename;
        return EXIT_FAILURE;
    }
    const QString emscriptenNativeHelpersJavascript = emscriptenNativeHelpersFile.readAll();

    const QString html = QString(originalHtml).replace("%1", javascript + emscriptenNativeHelpersJavascript);
    
    qDebug() << "Html " << html;

    QApplication app(argc, argv);
    QWebView *webView = new QWebView;
    webView->show();

    return app.exec();
}
