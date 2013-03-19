#include <QtGui/QApplication>
#include <QtWebKit/QWebView>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QWebView *webView = new QWebView;
    webView->show();

    return app.exec();
}
