#include "qprinterinfo.h"
#include "qprinterinfo_p.h"

#include <qstringlist.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_PRINTER


QList<QPrinterInfo> QPrinterInfo::availablePrinters()
{
    QList<QPrinterInfo> printers;
    return printers;
}

QPrinterInfo QPrinterInfo::defaultPrinter()
{
    return QPrinterInfo();
}

QList<QPrinter::PaperSize> QPrinterInfo::supportedPaperSizes() const
{
    const Q_D(QPrinterInfo);

    QList<QPrinter::PaperSize> paperSizes;
    if (isNull())
        return paperSizes;

    return paperSizes;
}

#endif // QT_NO_PRINTER

QT_END_NAMESPACE
