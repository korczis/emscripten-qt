#include <QtCore/QObject>
#include <QtGui/QImage>

class QPainter;

class Html5GraphicsSystemTests : public QObject
{
Q_OBJECT
public:
    Html5GraphicsSystemTests(int widgetWidth, int widgetHeight);
public slots:
    void testSanityTest();
    void testTwoFilledRectangles();
    void testDrawRectangleDefaultPenAndBrush();
    void testDrawRectangleDefaultPenAndBrush2();
    void testDrawRectangleRedPenAndDefaultBrush();
    void testDrawRectangleRedPenAndBlueBrush();
    void testDrawRectangleWithThickLine();
    void testSaveAndRestoreDrawingState();
    void testSingleClippedRectangle();
    void testTwoClippedRectangles();
    void testChangingClipDoesntUndoPenAndBrush();
    void testChangingClipDoesntUndoSubsequentPenAndBrushChanges();
    void testDrawQPixmapWithVariableAlpha();
    void testDrawQPixmapWithRGB32Format();
    void testSaveAndRestoreDrawingStateIncludesClip();
    void testCanTranslateQPainter();
    void testSettingClipBeforeTranslateDoesNotAlterClip();
    void testClippingAfterTranslateGivesTranslatedClip();
    void testDrawRectangleAfterRotateBySixtyDegreesOnUntranslatedCanvas();
    void testDrawEllipseNoFill();
    void testDrawEllipse();
    void testFillWithTexturedQBrush();
    void testFillRectWithTexturedQBrush();
    void testDrawARGB32QImage();
    void testDrawStretchedPortionOfImage();
    void testDrawStretchedPortionOfPixmap();
    void testDraw8BitQImage();
    void testFillRectWithAngledLinearGradient();
    void testFillRectWithBuiltInPatterns();
    void testDrawLines();
    void testFillRectWithBuiltInPatternsHasTransparentBackground();
    void testFillRectWithBuiltInPatternsRespectsOpaqueBackgroundMode();
    void testSetComplexClipRegion();
    void testSetComplexClipRegionWorksAfterSettingSimpleClip();
    void testSetDisjointClipRegion();
    void testDoNotTryToRestoreClipFromEndedPainter();
    void testMoveToPointAndDrawBezier();
    void testDrawLineTo();
    void testFillPathsWithLinesAndBeziers();
    void testFillTextPath();
public:
    void setExpectedImage(const QImage& expectedImage);
    QImage expectedImage();
    void setPainterForTest(QPainter* painter);
    QPainter *painter();
    int widgetWidth();
    int widgetHeight();
private:
    QImage m_expectedImage;
    QPainter *m_painter;
    int m_widgetWidth;
    int m_widgetHeight;
};
