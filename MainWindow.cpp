#include "MainWindow.h"

#include "PuzzleEdgeDetector.h"
#include "PuzzleEdgeMatcher.h"

#include <QResizeEvent>
#include <QTimer>
#include <QGraphicsEllipseItem>
#include <QBrush>
#include <QCloseEvent>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QElapsedTimer>
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QFileInfo>
#include <QGraphicsPolygonItem>
#include <QListWidget>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsTextItem>
#include <QAction>
#include <QFileDialog>
#include <QFont>
#include <QGraphicsPixmapItem>
#include <array>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTransform>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{

class ImageView : public QGraphicsView
{
public:
    using QGraphicsView::QGraphicsView;

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        const double factor =
            event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;

        scale(factor, factor);
        event->accept();
    }
};

QImage extractPiece(const QImage& image, const PuzzlePiece& piece)
{
    if (piece.contour.isEmpty())
        return {};

    const QRect bounds = piece.contour.boundingRect()
                             .adjusted(-2, -2, 2, 2)
                             .toAlignedRect()
                             .intersected(image.rect());

    if (bounds.isEmpty())
        return {};

    QImage result(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addPolygon(piece.contour);
    path.closeSubpath();

    QTransform transform;
    transform.translate(-bounds.left(), -bounds.top());

    painter.setClipPath(transform.map(path));
    painter.drawImage(QPoint(-bounds.left(), -bounds.top()), image);

    return result;
}

QImage rotatePiece(const QImage& image, double angle)
{
    if (image.isNull())
        return {};

    QTransform transform;
    transform.rotate(angle);

    return image.transformed(transform, Qt::SmoothTransformation);
}

const std::array<QColor, 4>& edgeColors()
{
    static const std::array<QColor, 4> colors = {
        QColor("#00E5FF"),
        QColor("#FF9100"),
        QColor("#B388FF"),
        QColor("#76FF03")
    };

    return colors;
}

void drawContourEdges(
    QPainter& painter,
    const PuzzleContourEdges& edges,
    const QTransform& transform,
    double lineWidth,
    bool drawRealCorners)
{
    if (!edges.valid)
        return;

    const std::array<QColor, 4>& colors = edgeColors();

    painter.setBrush(Qt::NoBrush);

    for (int side = 0; side < edges.edges.size(); ++side)
    {
        painter.setPen(QPen(
            colors[side],
            lineWidth,
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin
            ));

        painter.drawPolyline(transform.map(edges.edges[side]));
    }

    if (!drawRealCorners)
        return;

    painter.setPen(QPen(Qt::black, lineWidth + 1.0));
    painter.setBrush(Qt::white);

    const double radius = std::max(3.0, lineWidth * 2.5);

    for (const QPointF& corner : edges.corners)
    {
        painter.drawEllipse(
            transform.map(corner),
            radius,
            radius
            );
    }
}

QString edgeTypeName(PuzzleEdgeType type)
{
    switch (type)
    {
    case PuzzleEdgeType::Flat:
        return QObject::tr("Gerade");

    case PuzzleEdgeType::Outward:
        return QObject::tr("Nase");

    case PuzzleEdgeType::Inward:
        return QObject::tr("Einbuchtung");

    case PuzzleEdgeType::Unknown:
        return QObject::tr("Unbekannt");
    }

    return QObject::tr("Unbekannt");
}

void drawEdgeLabels(
    QPainter& painter,
    const PuzzleContourEdges& edges,
    const QTransform& transform,
    double fontSize)
{
    if (!edges.valid)
        return;

    QFont font = painter.font();
    font.setPixelSize(std::max(9, static_cast<int>(fontSize)));
    font.setBold(true);

    painter.setFont(font);

    for (int side = 0; side < edges.edges.size(); ++side)
    {
        if (side >= edges.classifications.size())
            continue;

        const QPolygonF& contour = edges.edges[side];

        if (contour.isEmpty())
            continue;

        const QPointF middle =
            transform.map(contour[contour.size() / 2]);

        const QString label = edgeTypeName(
            edges.classifications[side].type
            );

        const QRectF textBounds =
            painter.fontMetrics().boundingRect(label);

        const QRectF background(
            middle.x() - textBounds.width() * 0.5 - 5.0,
            middle.y() - textBounds.height() * 0.5 - 3.0,
            textBounds.width() + 10.0,
            textBounds.height() + 6.0
            );

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 210));
        painter.drawRoundedRect(background, 4.0, 4.0);

        painter.setPen(edgeColors()[side]);
        painter.setBrush(Qt::NoBrush);

        painter.drawText(
            background,
            Qt::AlignCenter,
            label
            );
    }
}

double calculateLocalColorDifference(
    const PuzzleLabColor& first,
    const PuzzleLabColor& second,
    const PuzzleLabColor& firstTable,
    const PuzzleLabColor& secondTable,
    bool firstTableValid,
    bool secondTableValid)
{
    double correctionL = 0.0;
    double correctionA = 0.0;
    double correctionB = 0.0;

    if (firstTableValid && secondTableValid)
    {
        correctionL = std::clamp(firstTable.l - secondTable.l, -12.0, 12.0);
        correctionA = std::clamp(firstTable.a - secondTable.a, -8.0, 8.0);
        correctionB = std::clamp(firstTable.b - secondTable.b, -8.0, 8.0);
    }

    const double dl = first.l - (second.l + correctionL);
    const double da = first.a - (second.a + correctionA);
    const double db = first.b - (second.b + correctionB);

    return std::sqrt(dl * dl + da * da + db * db);
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    scene = new QGraphicsScene(this);
    secondScene = new QGraphicsScene(this);
    matchingScene = new QGraphicsScene(this);
    alignedScene = new QGraphicsScene(this);

    view = new ImageView(scene, this);
    secondView = new ImageView(secondScene, this);
    matchingView = new ImageView(matchingScene, this);
    alignedView = new ImageView(alignedScene, this);

    for (QGraphicsView* graphicsView :
         {view, secondView, matchingView, alignedView})
    {
        graphicsView->setRenderHint(QPainter::Antialiasing);
        graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
        graphicsView->setTransformationAnchor(
            QGraphicsView::AnchorUnderMouse
            );
        graphicsView->setBackgroundBrush(QColor(45, 48, 54));
    }

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    leftSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);

    // Linke Seite: Bilderliste

    QGroupBox* imagesGroup = new QGroupBox(tr("Bilder"), leftSplitter);
    QVBoxLayout* imagesLayout = new QVBoxLayout(imagesGroup);

    imageList = new QListWidget(imagesGroup);
    imageList->setSelectionMode(QAbstractItemView::SingleSelection);

    imagesLayout->addWidget(imageList);
    leftSplitter->addWidget(imagesGroup);

    // Linke Seite: Match-Liste

    QGroupBox* matchesGroup = new QGroupBox(tr("Matches"), leftSplitter);
    QVBoxLayout* matchesLayout = new QVBoxLayout(matchesGroup);

    matchingList = new QListWidget(matchesGroup);
    matchingList->setSelectionMode(QAbstractItemView::SingleSelection);
    matchingList->setIconSize(QSize(64, 64));
    matchingList->setSpacing(3);

    matchesLayout->addWidget(matchingList);
    leftSplitter->addWidget(matchesGroup);

    // Rechte Seite: Originalbild A

    QGroupBox* firstImageGroup =
        new QGroupBox(tr("Originalbild A"), rightSplitter);

    QVBoxLayout* firstImageLayout =
        new QVBoxLayout(firstImageGroup);

    firstImageLayout->setContentsMargins(4, 4, 4, 4);
    firstImageLayout->addWidget(view);

    rightSplitter->addWidget(firstImageGroup);

    // Rechte Seite: Originalbild B

    QGroupBox* secondImageGroup =
        new QGroupBox(tr("Originalbild B"), rightSplitter);

    QVBoxLayout* secondImageLayout =
        new QVBoxLayout(secondImageGroup);

    secondImageLayout->setContentsMargins(4, 4, 4, 4);
    secondImageLayout->addWidget(secondView);

    rightSplitter->addWidget(secondImageGroup);

    // Rechte Seite: zusammengesetzte Match-Vorschau

    QGroupBox* previewGroup =
        new QGroupBox(tr("Match-Vorschau"), rightSplitter);

    QVBoxLayout* previewLayout =
        new QVBoxLayout(previewGroup);

    previewLayout->setContentsMargins(4, 4, 4, 4);
    previewLayout->addWidget(matchingView);

    rightSplitter->addWidget(previewGroup);

    // Splitter konfigurieren

    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(rightSplitter);

    mainSplitter->setChildrenCollapsible(false);
    leftSplitter->setChildrenCollapsible(false);
    rightSplitter->setChildrenCollapsible(false);

    mainSplitter->setStretchFactor(0, 4);
    mainSplitter->setStretchFactor(1, 6);

    leftSplitter->setStretchFactor(0, 2);
    leftSplitter->setStretchFactor(1, 8);

    rightSplitter->setStretchFactor(0, 1);
    rightSplitter->setStretchFactor(1, 1);
    rightSplitter->setStretchFactor(2, 1);

    mainSplitter->setSizes({450, 750});
    leftSplitter->setSizes({200, 700});
    rightSplitter->setSizes({300, 300, 300});

    setCentralWidget(mainSplitter);

    // Match-Auswahl

    connect(
        matchingList,
        &QListWidget::currentRowChanged,
        this,
        [this](int row)
        {
            showMatchingPreview(row);
            highlightMatchingPieces(row);
        }
        );

    connect(
        rightSplitter,
        &QSplitter::splitterMoved,
        this,
        [this](int, int)
        {
            fitOriginalImages();
        }
        );

    connect(
        mainSplitter,
        &QSplitter::splitterMoved,
        this,
        [this](int, int)
        {
            fitOriginalImages();
        }
        );

    // Aktionen

    QAction* openAction =
        new QAction(tr("Bild öffnen"), this);

    QAction* detectAction =
        new QAction(tr("Teile erkennen"), this);

    connect(
        openAction,
        &QAction::triggered,
        this,
        [this]()
        {
            openImage();
        }
        );

    connect(
        detectAction,
        &QAction::triggered,
        this,
        [this]()
        {
            detectPieces();
        }
        );

    QMenu* fileMenu = menuBar()->addMenu(tr("Datei"));
    fileMenu->addAction(openAction);

    QToolBar* toolBar = addToolBar(tr("Werkzeuge"));
    toolBar->addAction(openAction);
    toolBar->addAction(detectAction);

    resize(1500, 900);
    setWindowTitle(tr("JigsawFoundry"));

    restoreLayout();

    statusBar()->showMessage(tr("Bitte ein Puzzlefoto öffnen."));
}

void MainWindow::restoreLayout()
{
    QSettings settings("MZ Software GmbH", "JigsawFoundry");

    const QByteArray mainState =
        settings.value("MainWindow/MainSplitter").toByteArray();

    const QByteArray leftState =
        settings.value("MainWindow/LeftSplitter").toByteArray();

    const QByteArray rightState =
        settings.value("MainWindow/RightSplitter").toByteArray();

    if (!mainState.isEmpty())
        mainSplitter->restoreState(mainState);

    if (!leftState.isEmpty())
        leftSplitter->restoreState(leftState);

    if (!rightState.isEmpty())
        rightSplitter->restoreState(rightState);

    const QByteArray geometry =
        settings.value("MainWindow/Geometry").toByteArray();

    if (!geometry.isEmpty())
        restoreGeometry(geometry);
}

void MainWindow::saveLayout()
{
    QSettings settings("MZ Software GmbH", "JigsawFoundry");

    settings.setValue(
        "MainWindow/MainSplitter",
        mainSplitter->saveState()
        );

    settings.setValue(
        "MainWindow/LeftSplitter",
        leftSplitter->saveState()
        );

    settings.setValue(
        "MainWindow/RightSplitter",
        rightSplitter->saveState()
        );

    settings.setValue(
        "MainWindow/Geometry",
        saveGeometry()
        );
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveLayout();
    QMainWindow::closeEvent(event);
}

void MainWindow::openImage()
{
    QSettings settings("MZ Software GmbH", "JigsawFoundry");

    const QString lastDirectory = settings.value(
                                              "Image/LastDirectory",
                                              QDir::homePath()
                                              ).toString();

    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        tr("Puzzlefotos öffnen"),
        lastDirectory,
        tr("Bilder (*.jpg *.jpeg *.png *.bmp)")
        );

    if (paths.isEmpty())
        return;

    PuzzleDetector detector;
    PuzzleEdgeDetector edgeDetector;

    int addedImages = 0;

    for (const QString& path : paths)
    {
        const QString absolutePath =
            QFileInfo(path).canonicalFilePath();

        if (absolutePath.isEmpty())
            continue;

        bool alreadyLoaded = false;

        for (const PuzzleImage& existing : images)
        {
            if (existing.filePath == absolutePath)
            {
                alreadyLoaded = true;
                break;
            }
        }

        if (alreadyLoaded)
            continue;

        PuzzleImage puzzleImage;
        puzzleImage.filePath = absolutePath;
        puzzleImage.image = QImage(absolutePath);

        if (puzzleImage.image.isNull())
        {
            qWarning() << "Bild konnte nicht geladen werden:"
                       << absolutePath;
            continue;
        }

        analyzeImage(puzzleImage);

        images.append(puzzleImage);
        ++addedImages;
    }

    if (addedImages == 0)
    {
        statusBar()->showMessage(
            tr("Keine neuen Bilder hinzugefügt.")
            );
        return;
    }

    settings.setValue(
        "Image/LastDirectory",
        QFileInfo(paths.last()).absolutePath()
        );

    updateImageList();
    updateMatching();

    statusBar()->showMessage(
        tr("%1 Bilder geladen, davon %2 neu.")
            .arg(images.size())
            .arg(addedImages)
        );
}

void MainWindow::updateImageList()
{
    imageList->clear();

    for (const PuzzleImage& puzzleImage : images)
    {
        const QString name =
            QFileInfo(puzzleImage.filePath).fileName();

        QListWidgetItem* item = new QListWidgetItem(
            tr("%1 – %2 gültige Teile")
                .arg(name)
                .arg(puzzleImage.usablePieces)
            );

        item->setIcon(QIcon(puzzleImage.thumbnail));

        imageList->addItem(item);
    }
}

void MainWindow::detectPieces()
{
    if (images.isEmpty())
    {
        statusBar()->showMessage(tr("Bitte zuerst Bilder öffnen."));
        return;
    }

    QElapsedTimer timer;
    timer.start();

    for (PuzzleImage& puzzleImage : images)
        analyzeImage(puzzleImage);

    displayedFirstImageIndex = -1;
    displayedSecondImageIndex = -1;

    updateImageList();
    updateMatching();

    statusBar()->showMessage(
        tr("%1 Bilder neu analysiert in %2 ms.")
            .arg(images.size())
            .arg(timer.elapsed())
        );
}

void MainWindow::showAlignedPieces(int imageIndex)
{
    if (imageIndex < 0 || imageIndex >= images.size())
        return;

    const PuzzleImage& puzzleImage = images[imageIndex];

    const QImage& image = puzzleImage.image;
    const QVector<PuzzlePiece>& pieces = puzzleImage.pieces;
    const QVector<PuzzleContourEdges>& pieceAnalyses = puzzleImage.analyses;
    const QVector<double>& pieceRotations = puzzleImage.rotations;
    const QVector<PuzzleBaseLines>& pieceBaseLines = puzzleImage.baseLines;

    alignedScene->clear();

    if (image.isNull() || pieces.isEmpty())
        return;

    const int columns = 5;
    const double cellWidth = 260.0;
    const double cellHeight = 280.0;
    const double padding = 20.0;

    int itemIndex = 0;

    for (int pieceIndex = 0; pieceIndex < pieces.size(); ++pieceIndex)
    {
        const PuzzlePiece& piece = pieces[pieceIndex];
        const double rotation = pieceRotations[pieceIndex];
        const PuzzleBaseLines& lines = pieceBaseLines[pieceIndex];
        const PuzzleContourEdges& edges = pieceAnalyses[pieceIndex];

        QImage extracted = extractPiece(image, piece);

        if (extracted.isNull())
            continue;

        QImage rotated = rotatePiece(extracted, rotation);

        if (rotated.isNull())
            continue;

        const QRect bounds = piece.contour.boundingRect()
                                 .adjusted(-2, -2, 2, 2)
                                 .toAlignedRect()
                                 .intersected(image.rect());

        QTransform rotationTransform;
        rotationTransform.rotate(rotation);

        const QRectF transformedBounds =
            rotationTransform.mapRect(QRectF(
                0.0,
                0.0,
                extracted.width(),
                extracted.height()
                ));

        const QPointF globalImageOrigin =
            rotationTransform.map(QPointF(bounds.topLeft()))
            + transformedBounds.topLeft();

        const double offsetX = globalImageOrigin.x();
        const double offsetY = globalImageOrigin.y();

        // Convert original image coordinates directly into
        // coordinates of the rotated, extracted image.
        QTransform contourTransform;

        contourTransform.translate(
            -transformedBounds.left(),
            -transformedBounds.top()
            );

        contourTransform.rotate(rotation);

        contourTransform.translate(
            -bounds.left(),
            -bounds.top()
            );

        QPainter painter(&rotated);
        painter.setRenderHint(QPainter::Antialiasing);

        if (lines.valid)
        {
            const double left = lines.left - offsetX;
            const double right = lines.right - offsetX;
            const double top = lines.top - offsetY;
            const double bottom = lines.bottom - offsetY;

            // Theoretical base lines: red.
            painter.setPen(QPen(
                QColor("#FF1744"),
                1.5,
                Qt::DashLine
                ));

            painter.setBrush(Qt::NoBrush);

            painter.drawLine(
                QPointF(left, 0.0),
                QPointF(left, rotated.height())
                );

            painter.drawLine(
                QPointF(right, 0.0),
                QPointF(right, rotated.height())
                );

            painter.drawLine(
                QPointF(0.0, top),
                QPointF(rotated.width(), top)
                );

            painter.drawLine(
                QPointF(0.0, bottom),
                QPointF(rotated.width(), bottom)
                );

            // Theoretical corners: yellow.
            painter.setPen(QPen(Qt::black, 1.5));
            painter.setBrush(QColor("#FFEA00"));

            const QPointF theoreticalCorners[4] = {
                QPointF(left, top),
                QPointF(right, top),
                QPointF(right, bottom),
                QPointF(left, bottom)
            };

            for (const QPointF& corner : theoreticalCorners)
                painter.drawEllipse(corner, 3.5, 3.5);
        }

        // Four actual contour edges, with the same colors
        // used in the original image.
        drawContourEdges(
            painter,
            edges,
            contourTransform,
            2.5,
            true
            );
        drawEdgeLabels(
            painter,
            edges,
            contourTransform,
            13.0
            );

        painter.end();

        const int column = itemIndex % columns;
        const int row = itemIndex / columns;

        const double cellX = column * cellWidth;
        const double cellY = row * cellHeight;

        const double availableWidth = cellWidth - padding * 2.0;
        const double availableHeight = cellHeight - 65.0;

        const double scaleFactor = std::min(
            1.0,
            std::min(
                availableWidth / rotated.width(),
                availableHeight / rotated.height()
                )
            );

        const double displayWidth = rotated.width() * scaleFactor;
        const double displayHeight = rotated.height() * scaleFactor;

        const double imageX =
            cellX + (cellWidth - displayWidth) / 2.0;

        const double imageY =
            cellY + 35.0 +
            (availableHeight - displayHeight) / 2.0;

        QGraphicsPixmapItem* item =
            alignedScene->addPixmap(QPixmap::fromImage(rotated));

        item->setScale(scaleFactor);
        item->setPos(imageX, imageY);

        QString labelText = tr("Teil %1 – %2°")
                                .arg(piece.id)
                                .arg(rotation, 0, 'f', 1);

        if (!edges.valid)
            labelText += tr(" – Ecken nicht erkannt");
        else if (!edges.usableForMatching)
            labelText += tr(" – Ausgeschlossen");

        QGraphicsTextItem* label =
            alignedScene->addText(labelText);

        label->setDefaultTextColor(Qt::white);
        label->setPos(cellX + 10.0, cellY + 5.0);

        ++itemIndex;
    }

    const int rows = (itemIndex + columns - 1) / columns;

    alignedScene->setSceneRect(
        0.0,
        0.0,
        columns * cellWidth,
        rows * cellHeight
        );

    if (itemIndex > 0)
    {
        alignedView->fitInView(
            alignedScene->sceneRect(),
            Qt::KeepAspectRatio
            );
    }
}

void MainWindow::updateMatching()
{
    matchingList->clear();
    matchingScene->clear();
    matches.clear();

    PuzzleEdgeMatcher matcher;
    matcher.calibrateImages(images);
    matcher.calibrateTableColors(images);

    for (int index = 0; index < images.size(); ++index)
    {
        const PuzzleImage& puzzleImage = images[index];

        qDebug().noquote()
            << QStringLiteral("Bild %1: %2 | Teile: %3 | Kantenlänge: %4 px | Skalierung: %5 | Gültig: %6")
                   .arg(index + 1)
                   .arg(QFileInfo(puzzleImage.filePath).fileName())
                   .arg(puzzleImage.usablePieces)
                   .arg(puzzleImage.typicalEdgeLength, 0, 'f', 2)
                   .arg(puzzleImage.scaleFactor, 0, 'f', 4)
                   .arg(puzzleImage.scaleValid);
    }

    for (int index = 0; index < images.size(); ++index)
    {
        const PuzzleImage& puzzleImage = images[index];

        qDebug().noquote()
            << QStringLiteral("Bild %1 | Tisch Lab: L=%2 a=%3 b=%4 | Gültig: %5")
                   .arg(index + 1)
                   .arg(puzzleImage.tableColor.l, 0, 'f', 2)
                   .arg(puzzleImage.tableColor.a, 0, 'f', 2)
                   .arg(puzzleImage.tableColor.b, 0, 'f', 2)
                   .arg(puzzleImage.tableColorValid);
    }

    matches = matcher.findMatches(images);

    const int maximumResults =
        std::min(200, static_cast<int>(matches.size()));

    for (int index = 0; index < maximumResults; ++index)
    {
        const PuzzleEdgeMatch& match = matches[index];

        const PuzzleImage& firstImage = images[match.firstImageIndex];
        const PuzzleImage& secondImage = images[match.secondImageIndex];

        const PuzzlePiece& first = firstImage.pieces[match.firstPieceIndex];
        const PuzzlePiece& second = secondImage.pieces[match.secondPieceIndex];

        const QString firstName = QFileInfo(firstImage.filePath).fileName();
        const QString secondName = QFileInfo(secondImage.filePath).fileName();

        const QString description =
            tr("%1: Teil %2 / Kante %3\n"
               "%4: Teil %5 / Kante %6\n"
               "G: %7  K: %8  Gmax: %9  Gnase: %10\n"
               "L: %11%  F: %12  FK: %13  Gesamt: %14")
                .arg(firstName)
                .arg(first.id)
                .arg(match.firstEdgeIndex + 1)
                .arg(secondName)
                .arg(second.id)
                .arg(match.secondEdgeIndex + 1)
                .arg(match.geometryScore, 0, 'f', 2)
                .arg(match.contourScore, 0, 'f', 2)
                .arg(match.maximumGeometryScore, 0, 'f', 2)
                .arg(match.noseGeometryScore, 0, 'f', 2)
                .arg(match.lengthScore, 0, 'f', 2)
                .arg(match.colorScore, 0, 'f', 2)
                .arg(match.calibratedColorScore, 0, 'f', 2)
                .arg(match.score, 0, 'f', 2);

        QPixmap preview(152, 80);
        preview.fill(QColor(45, 48, 54));

        QPainter painter(&preview);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const QPixmap& firstThumbnail =
            firstImage.pieceThumbnails[match.firstPieceIndex];

        const QPixmap& secondThumbnail =
            secondImage.pieceThumbnails[match.secondPieceIndex];

        const QPixmap firstScaled =
            firstThumbnail.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        const QPixmap secondScaled =
            secondThumbnail.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        painter.drawPixmap(
            (76 - firstScaled.width()) / 2,
            (80 - firstScaled.height()) / 2,
            firstScaled
            );

        painter.drawPixmap(
            76 + (76 - secondScaled.width()) / 2,
            (80 - secondScaled.height()) / 2,
            secondScaled
            );

        painter.end();

        QListWidgetItem* item = new QListWidgetItem(
            QIcon(preview),
            description
            );

        if (match.mutualBestMatch)
        {
            item->setForeground(QColor("#FFD600"));

            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }

        matchingList->addItem(item);
    }

    matchingList->setIconSize(QSize(152, 80));

    if (maximumResults > 0)
    {
        matchingList->setCurrentRow(0);
        showMatchingPreview(0);
        highlightMatchingPieces(0);
    }
    else
    {
        highlightMatchingPieces(-1);
    }

    statusBar()->showMessage(
        tr("%1 Bilder, %2 mögliche Kantenpaare")
            .arg(images.size())
            .arg(matches.size())
        );
}

void MainWindow::showMatchingPreview(int matchIndex)
{
    matchingScene->clear();

    if (matchIndex < 0 ||
        matchIndex >= matchingList->count() ||
        matchIndex >= matches.size())
        return;

    const PuzzleEdgeMatch& match = matches[matchIndex];

    if (match.firstImageIndex < 0 ||
        match.firstImageIndex >= images.size() ||
        match.secondImageIndex < 0 ||
        match.secondImageIndex >= images.size())
        return;

    const PuzzleImage& firstSource = images[match.firstImageIndex];
    const PuzzleImage& secondSource = images[match.secondImageIndex];

    if (match.firstPieceIndex < 0 ||
        match.firstPieceIndex >= firstSource.pieces.size() ||
        match.secondPieceIndex < 0 ||
        match.secondPieceIndex >= secondSource.pieces.size())
        return;

    const PuzzlePiece& first = firstSource.pieces[match.firstPieceIndex];
    const PuzzlePiece& second = secondSource.pieces[match.secondPieceIndex];

    const PuzzleContourEdges& firstAnalysis = firstSource.analyses[match.firstPieceIndex];
    const PuzzleContourEdges& secondAnalysis = secondSource.analyses[match.secondPieceIndex];

    if (match.firstEdgeIndex < 0 ||
        match.firstEdgeIndex >= firstAnalysis.edges.size() ||
        match.secondEdgeIndex < 0 ||
        match.secondEdgeIndex >= secondAnalysis.edges.size())
        return;

    const QPolygonF& firstEdge = firstAnalysis.edges[match.firstEdgeIndex];
    const QPolygonF& secondEdge = secondAnalysis.edges[match.secondEdgeIndex];

    if (firstEdge.size() < 2 || secondEdge.size() < 2)
        return;

    const QImage& firstImage = firstSource.extractedPieces[match.firstPieceIndex];
    const QImage& secondImage = secondSource.extractedPieces[match.secondPieceIndex];

    if (firstImage.isNull() || secondImage.isNull())
        return;

    // ------------------------------------------------------------
    // 1. Beide Teile anhand der Kanten ausrichten
    // ------------------------------------------------------------

    const QPointF firstStart = firstEdge.first();
    const QPointF firstEnd = firstEdge.last();
    const QPointF secondStart = secondEdge.first();
    const QPointF secondEnd = secondEdge.last();

    const double firstAngle = std::atan2(firstStart.y() - firstEnd.y(), firstStart.x() - firstEnd.x());
    const double secondAngle = std::atan2(secondEnd.y() - secondStart.y(), secondEnd.x() - secondStart.x());

    const double firstScale = firstSource.scaleFactor;
    const double secondScale = secondSource.scaleFactor;
    const double relativeScale = firstScale > 0.0 ? secondScale / firstScale : 1.0;

    QTransform secondRotation;
    secondRotation.rotateRadians(firstAngle - secondAngle);

    QTransform secondTransform;
    secondTransform.rotateRadians(firstAngle - secondAngle);
    secondTransform.scale(relativeScale, relativeScale);

    const QPointF firstOffset = -first.center;

    const QPointF firstMidpoint = (firstStart + firstEnd) * 0.5;
    const QPointF secondMidpoint = (secondStart + secondEnd) * 0.5;

    // Beide Sehnenmittelpunkte zur Deckung bringen.
    // Unterschiede der Sehnenlängen bleiben sichtbar.
    const QPointF secondOffset =
        firstMidpoint + firstOffset - secondTransform.map(secondMidpoint);

    const QRect firstBounds = first.contour.boundingRect()
                                  .adjusted(-2, -2, 2, 2)
                                  .toAlignedRect()
                                  .intersected(firstSource.image.rect());

    const QRect secondBounds = second.contour.boundingRect()
                                   .adjusted(-2, -2, 2, 2)
                                   .toAlignedRect()
                                   .intersected(secondSource.image.rect());

    // ------------------------------------------------------------
    // 2. Halbtransparente Puzzleteile anzeigen
    // ------------------------------------------------------------

    QGraphicsPixmapItem* firstItem =
        matchingScene->addPixmap(QPixmap::fromImage(firstImage));

    firstItem->setPos(QPointF(firstBounds.topLeft()) + firstOffset);
    firstItem->setOpacity(0.75);
    firstItem->setZValue(1.0);

    QGraphicsPixmapItem* secondItem =
        matchingScene->addPixmap(QPixmap::fromImage(secondImage));

    secondItem->setTransform(secondTransform);
    secondItem->setPos(secondTransform.map(QPointF(secondBounds.topLeft())) + secondOffset);

    secondItem->setOpacity(0.65);
    secondItem->setZValue(2.0);

    // ------------------------------------------------------------
    // 3. Beide Kanten ins gemeinsame Koordinatensystem bringen
    // ------------------------------------------------------------

    QPolygonF firstPreviewEdge;
    QPolygonF secondPreviewEdge;

    for (const QPointF& point : firstEdge)
        firstPreviewEdge.append(point + firstOffset);

    for (const QPointF& point : secondEdge)
        secondPreviewEdge.append(secondTransform.map(point) + secondOffset);

    // ------------------------------------------------------------
    // 4. Kanten zeichnen
    // ------------------------------------------------------------

    QPainterPath firstPath;
    firstPath.moveTo(firstPreviewEdge.first());

    for (int index = 1; index < firstPreviewEdge.size(); ++index)
        firstPath.lineTo(firstPreviewEdge[index]);

    QPen firstPen(QColor("#00E5FF"), 2.5);
    firstPen.setCosmetic(true);

    QGraphicsPathItem* firstLine =
        matchingScene->addPath(firstPath, firstPen);

    firstLine->setZValue(10.0);

    QPainterPath secondPath;
    secondPath.moveTo(secondPreviewEdge.first());

    for (int index = 1; index < secondPreviewEdge.size(); ++index)
        secondPath.lineTo(secondPreviewEdge[index]);

    QPen secondPen(QColor("#FF9100"), 2.5);
    secondPen.setCosmetic(true);

    QGraphicsPathItem* secondLine =
        matchingScene->addPath(secondPath, secondPen);

    secondLine->setZValue(11.0);

    // ------------------------------------------------------------
    // 5. Entsprechende Punkte entlang der Kanten bestimmen
    // ------------------------------------------------------------

    const int sampleCount = 32;

    const double firstLength = [&firstPreviewEdge]()
    {
        double length = 0.0;

        for (int index = 1; index < firstPreviewEdge.size(); ++index)
        {
            const QPointF difference =
                firstPreviewEdge[index] - firstPreviewEdge[index - 1];

            length += std::hypot(difference.x(), difference.y());
        }

        return length;
    }();

    const double secondLength = [&secondPreviewEdge]()
    {
        double length = 0.0;

        for (int index = 1; index < secondPreviewEdge.size(); ++index)
        {
            const QPointF difference =
                secondPreviewEdge[index] - secondPreviewEdge[index - 1];

            length += std::hypot(difference.x(), difference.y());
        }

        return length;
    }();

    const double averageLength = (firstLength + secondLength) * 0.5;

    const double warningDistance = averageLength * 0.015;
    const double errorDistance = averageLength * 0.035;

    const double firstStartToEnd = firstLength;
    const double secondStartToEnd = secondLength;

    const QPointF firstFallback = firstPreviewEdge.first();
    const QPointF secondFallback = secondPreviewEdge.last();

    const int firstCount = firstPreviewEdge.size();
    const int secondCount = secondPreviewEdge.size();

    const QPolygonF& firstPoints = firstPreviewEdge;
    const QPolygonF& secondPoints = secondPreviewEdge;

    const double firstTotal = firstStartToEnd;
    const double secondTotal = secondStartToEnd;

    // Hilfsfunktion: Punkt anhand der relativen Bogenlänge bestimmen.
    const auto pointAtFraction = [](
                                     const QPolygonF& points,
                                     double fraction,
                                     double totalLength) -> QPointF
    {
        if (points.isEmpty())
            return QPointF();

        if (fraction <= 0.0)
            return points.first();

        if (fraction >= 1.0)
            return points.last();

        const double target = fraction * totalLength;
        double traveled = 0.0;

        for (int index = 1; index < points.size(); ++index)
        {
            const QPointF start = points[index - 1];
            const QPointF end = points[index];
            const QPointF delta = end - start;

            const double segmentLength =
                std::hypot(delta.x(), delta.y());

            if (traveled + segmentLength >= target)
            {
                const double t = segmentLength > 1e-9
                                     ? (target - traveled) / segmentLength
                                     : 0.0;

                return start + delta * t;
            }

            traveled += segmentLength;
        }

        return points.last();
    };

    // ------------------------------------------------------------
    // 6. Abweichungen zwischen den Konturen anzeigen
    // ------------------------------------------------------------

    for (int index = 1; index < sampleCount - 1; ++index)
    {
        const double fraction =
            static_cast<double>(index) / (sampleCount - 1);

        const QPointF firstPoint =
            pointAtFraction(firstPoints, fraction, firstTotal);

        // Die zweite Kante läuft in Gegenrichtung.
        const QPointF secondPoint =
            pointAtFraction(secondPoints, 1.0 - fraction, secondTotal);

        const QPointF difference = secondPoint - firstPoint;

        const double deviation =
            std::hypot(difference.x(), difference.y());

        QColor color("#66BB6A");

        if (deviation >= errorDistance)
            color = QColor("#FF1744");
        else if (deviation >= warningDistance)
            color = QColor("#FFD600");

        QPen connectionPen(color, 1.5);
        connectionPen.setCosmetic(true);

        QGraphicsLineItem* connection =
            matchingScene->addLine(
                QLineF(firstPoint, secondPoint),
                connectionPen
                );

        connection->setZValue(15.0);

        if (deviation >= warningDistance)
        {
            QGraphicsEllipseItem* marker =
                matchingScene->addEllipse(
                    QRectF(firstPoint.x() - 2.5,
                           firstPoint.y() - 2.5,
                           5.0,
                           5.0),
                    QPen(Qt::NoPen),
                    QBrush(color)
                    );

            marker->setZValue(16.0);
        }
    }

    // ------------------------------------------------------------
    // Lokale Farbabweichungen entlang der Verbindung anzeigen
    // ------------------------------------------------------------

    const PuzzleCachedEdge* firstCachedEdge = nullptr;
    const PuzzleCachedEdge* secondCachedEdge = nullptr;

    for (const PuzzleCachedEdge& edge : firstSource.cachedEdges)
    {
        if (edge.pieceIndex == match.firstPieceIndex &&
            edge.edgeIndex == match.firstEdgeIndex)
        {
            firstCachedEdge = &edge;
            break;
        }
    }

    for (const PuzzleCachedEdge& edge : secondSource.cachedEdges)
    {
        if (edge.pieceIndex == match.secondPieceIndex &&
            edge.edgeIndex == match.secondEdgeIndex)
        {
            secondCachedEdge = &edge;
            break;
        }
    }

    if (firstCachedEdge != nullptr && secondCachedEdge != nullptr)
    {
        const PuzzleEdgeColorProfile& firstColors = firstCachedEdge->colors;
        const PuzzleEdgeColorProfile& secondColors = secondCachedEdge->colors;

        const int colorSampleCount = 64;

        if (firstColors.values.size() == colorSampleCount &&
            secondColors.values.size() == colorSampleCount &&
            firstColors.valid.size() == colorSampleCount &&
            secondColors.valid.size() == colorSampleCount)
        {
            for (int index = 0; index < colorSampleCount; ++index)
            {
                const int opposite = colorSampleCount - 1 - index;

                if (!firstColors.valid[index] || !secondColors.valid[opposite])
                    continue;

                const double difference = calculateLocalColorDifference(
                    firstColors.values[index],
                    secondColors.values[opposite],
                    firstSource.tableColor,
                    secondSource.tableColor,
                    firstSource.tableColorValid,
                    secondSource.tableColorValid
                    );

                QColor color("#66BB6A");

                if (difference >= 20.0)
                    color = QColor("#FF1744");
                else if (difference >= 10.0)
                    color = QColor("#FFD600");

                const double fraction =
                    static_cast<double>(index) / (colorSampleCount - 1);

                const QPointF firstPoint =
                    pointAtFraction(firstPoints, fraction, firstTotal);

                const QPointF secondPoint =
                    pointAtFraction(secondPoints, 1.0 - fraction, secondTotal);

                const QPointF middle = (firstPoint + secondPoint) * 0.5;

                QGraphicsEllipseItem* marker =
                    matchingScene->addEllipse(
                        QRectF(middle.x() - 2.0,
                               middle.y() - 2.0,
                               4.0,
                               4.0),
                        QPen(Qt::NoPen),
                        QBrush(color)
                        );

                marker->setZValue(25.0);
            }
        }
    }

    // ------------------------------------------------------------
    // 7. Beschriftung
    // ------------------------------------------------------------

    QGraphicsTextItem* firstLabel =
        matchingScene->addText(tr("Teil %1 – Nase").arg(first.id));

    firstLabel->setDefaultTextColor(QColor("#00E5FF"));
    firstLabel->setPos(first.center + firstOffset);
    firstLabel->setZValue(20.0);

    QGraphicsTextItem* secondLabel =
        matchingScene->addText(tr("Teil %1 – Einbuchtung").arg(second.id));

    secondLabel->setDefaultTextColor(QColor("#FF9100"));
    secondLabel->setPos(secondTransform.map(second.center) + secondOffset);
    secondLabel->setZValue(20.0);

    // ------------------------------------------------------------
    // 8. Ansicht anpassen
    // ------------------------------------------------------------

    const QRectF bounds =
        matchingScene->itemsBoundingRect().adjusted(-30.0, -30.0, 30.0, 30.0);

    matchingScene->setSceneRect(bounds);
    matchingView->fitInView(bounds, Qt::KeepAspectRatio);
}

void MainWindow::highlightMatchingPieces(int matchIndex)
{
    if (matchIndex < 0 || matchIndex >= matches.size())
    {
        updateOriginalHighlight(firstMatchingHighlight, nullptr);
        updateOriginalHighlight(secondMatchingHighlight, nullptr);
        return;
    }

    const PuzzleEdgeMatch& match = matches[matchIndex];

    if (match.firstImageIndex < 0 ||
        match.firstImageIndex >= images.size() ||
        match.secondImageIndex < 0 ||
        match.secondImageIndex >= images.size())
        return;

    const PuzzleImage& firstImage =
        images[match.firstImageIndex];

    const PuzzleImage& secondImage =
        images[match.secondImageIndex];

    if (match.firstPieceIndex < 0 ||
        match.firstPieceIndex >= firstImage.pieces.size() ||
        match.secondPieceIndex < 0 ||
        match.secondPieceIndex >= secondImage.pieces.size())
        return;

    if (displayedFirstImageIndex != match.firstImageIndex)
    {
        displayOriginalImage(
            match.firstImageIndex,
            scene,
            firstImageItem,
            firstMatchingHighlight,
            QColor("#00FFFF")
            );

        displayedFirstImageIndex = match.firstImageIndex;

        view->fitInView(
            scene->sceneRect(),
            Qt::KeepAspectRatio
            );
    }

    if (displayedSecondImageIndex != match.secondImageIndex)
    {
        displayOriginalImage(
            match.secondImageIndex,
            secondScene,
            secondImageItem,
            secondMatchingHighlight,
            QColor("#FF9800")
            );

        displayedSecondImageIndex = match.secondImageIndex;

        secondView->fitInView(
            secondScene->sceneRect(),
            Qt::KeepAspectRatio
            );
    }

    updateOriginalHighlight(
        firstMatchingHighlight,
        &firstImage.pieces[match.firstPieceIndex]
        );

    updateOriginalHighlight(
        secondMatchingHighlight,
        &secondImage.pieces[match.secondPieceIndex]
        );
}

void MainWindow::fitOriginalImages()
{
    if (!scene->sceneRect().isEmpty())
        view->fitInView(
            scene->sceneRect(),
            Qt::KeepAspectRatio
            );

    if (!secondScene->sceneRect().isEmpty())
        secondView->fitInView(
            secondScene->sceneRect(),
            Qt::KeepAspectRatio
            );
}

void MainWindow::updateOriginalHighlight(
    QGraphicsPolygonItem* highlight,
    const PuzzlePiece* piece)
{
    if (highlight == nullptr)
        return;

    if (piece == nullptr || piece->contour.isEmpty())
    {
        highlight->hide();
        return;
    }

    highlight->setPolygon(piece->contour);
    highlight->show();
}

void MainWindow::analyzeImage(PuzzleImage& puzzleImage)
{
    puzzleImage.analyses.clear();
    puzzleImage.rotations.clear();
    puzzleImage.baseLines.clear();
    puzzleImage.extractedPieces.clear();
    puzzleImage.pieceThumbnails.clear();
    puzzleImage.cachedEdges.clear();

    // Der bisherige Code folgt.
    PuzzleDetector detector;
    PuzzleEdgeDetector edgeDetector;

    puzzleImage.pieces =
        detector.detect(puzzleImage.image);

    puzzleImage.pixmap =
        QPixmap::fromImage(puzzleImage.image);

    puzzleImage.thumbnail = QPixmap::fromImage(
        puzzleImage.image.scaled(
            96,
            96,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            )
        );

    const int count = puzzleImage.pieces.size();

    puzzleImage.analyses.reserve(count);
    puzzleImage.rotations.reserve(count);
    puzzleImage.baseLines.reserve(count);
    puzzleImage.extractedPieces.reserve(count);
    puzzleImage.pieceThumbnails.reserve(count);

    puzzleImage.usablePieces = 0;

    for (const PuzzlePiece& piece : puzzleImage.pieces)
    {
        const double rotation =
            edgeDetector.findRotation(piece.contour);

        const PuzzleBaseLines lines =
            edgeDetector.findBaseLines(
                piece.contour,
                rotation
                );

        const PuzzleContourEdges edges =
            edgeDetector.findContourEdges(
                piece.contour,
                rotation,
                lines
                );

        puzzleImage.rotations.append(rotation);
        puzzleImage.baseLines.append(lines);
        puzzleImage.analyses.append(edges);

        if (edges.usableForMatching)
            ++puzzleImage.usablePieces;

        const QImage extracted =
            extractPiece(puzzleImage.image, piece);

        puzzleImage.extractedPieces.append(extracted);

        puzzleImage.pieceThumbnails.append(
            QPixmap::fromImage(
                extracted.scaled(
                    72,
                    72,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                    )
                )
            );
    }

    PuzzleEdgeMatcher matcher;
    matcher.prepareImage(puzzleImage);
}

void MainWindow::displayOriginalImage(
    int imageIndex,
    QGraphicsScene* targetScene,
    QGraphicsPixmapItem*& imageItem,
    QGraphicsPolygonItem*& highlight,
    const QColor& highlightColor)
{
    targetScene->clear();

    imageItem = nullptr;
    highlight = nullptr;

    if (imageIndex < 0 || imageIndex >= images.size())
        return;

    const PuzzleImage& puzzleImage = images[imageIndex];

    imageItem = targetScene->addPixmap(
        puzzleImage.pixmap
        );

    QTransform transform;

    if (puzzleImage.image.height() >
        puzzleImage.image.width())
    {
        transform.rotate(90.0);
    }

    imageItem->setTransform(transform);

    highlight = new QGraphicsPolygonItem(imageItem);

    QPen pen(highlightColor, 4.0);
    pen.setCosmetic(true);
    pen.setJoinStyle(Qt::RoundJoin);

    highlight->setPen(pen);

    highlight->setBrush(
        QColor(
            highlightColor.red(),
            highlightColor.green(),
            highlightColor.blue(),
            100
            )
        );

    highlight->setZValue(10.0);
    highlight->hide();

    targetScene->setSceneRect(
        transform.mapRect(
            QRectF(puzzleImage.image.rect())
            )
        );
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    QTimer::singleShot(0, this, [this]()
                       {
                           fitOriginalImages();
                       });
}
