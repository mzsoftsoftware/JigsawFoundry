#include "MainWindow.h"

#include "PuzzleEdgeDetector.h"
#include "PuzzleEdgeMatcher.h"

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

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    scene = new QGraphicsScene(this);
    alignedScene = new QGraphicsScene(this);

    view = new ImageView(scene, this);
    alignedView = new ImageView(alignedScene, this);

    view->setRenderHint(QPainter::Antialiasing);
    alignedView->setRenderHint(QPainter::Antialiasing);

    view->setDragMode(QGraphicsView::ScrollHandDrag);
    alignedView->setDragMode(QGraphicsView::ScrollHandDrag);

    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    alignedView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    alignedView->setBackgroundBrush(QColor(45, 48, 54));

    QSplitter* upperSplitter = new QSplitter(Qt::Horizontal, this);

    upperSplitter->addWidget(view);
    upperSplitter->addWidget(alignedView);

    upperSplitter->setStretchFactor(0, 1);
    upperSplitter->setStretchFactor(1, 1);

    QSplitter* matchingSplitter = new QSplitter(Qt::Horizontal, this);

    matchingList = new QListWidget(matchingSplitter);
    matchingList->setMinimumWidth(250);
    matchingScene = new QGraphicsScene(this);
    matchingView = new ImageView(matchingScene, matchingSplitter);
    matchingView->setBackgroundBrush(QColor(45, 48, 54));
    matchingView->setRenderHint(QPainter::Antialiasing);
    matchingView->setDragMode(QGraphicsView::ScrollHandDrag);
    matchingView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    matchingSplitter->addWidget(matchingList);
    matchingSplitter->addWidget(matchingView);
    matchingSplitter->setStretchFactor(0, 3);
    matchingSplitter->setStretchFactor(1, 7);
    matchingSplitter->setSizes({300, 700});

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

    QSplitter* mainSplitter = new QSplitter(Qt::Vertical, this);

    mainSplitter->addWidget(upperSplitter);
    mainSplitter->addWidget(matchingSplitter);

    mainSplitter->setStretchFactor(0, 6);
    mainSplitter->setStretchFactor(1, 4);
    mainSplitter->setSizes({600, 400});

    setCentralWidget(mainSplitter);

    QAction* openAction = new QAction(tr("Bild öffnen"), this);
    QAction* detectAction = new QAction(tr("Teile erkennen"), this);

    connect(openAction, &QAction::triggered, this, [this]() {
        openImage();
    });

    connect(detectAction, &QAction::triggered, this, [this]() {
        detectPieces();
    });

    QMenu* fileMenu = menuBar()->addMenu(tr("Datei"));
    fileMenu->addAction(openAction);

    QToolBar* toolBar = addToolBar(tr("Werkzeuge"));
    toolBar->addAction(openAction);
    toolBar->addAction(detectAction);

    statusBar()->showMessage(tr("Bitte ein Puzzlefoto öffnen."));

    resize(1500, 900);
    setWindowTitle(tr("PuzzleSolver 0.2 – Ausrichtung"));
}

void MainWindow::openImage()
{
    QSettings settings("MZ Software GmbH", "JigsawFoundry");

    const QString lastDirectory = settings.value(
                                              "Image/LastDirectory",
                                              QDir::homePath()
                                              ).toString();

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Puzzlefoto öffnen"),
        lastDirectory,
        tr("Bilder (*.jpg *.jpeg *.png *.bmp)")
        );

    if (path.isEmpty())
        return;

    QImage loaded(path);

    if (loaded.isNull())
    {
        statusBar()->showMessage(tr("Das Bild konnte nicht geladen werden."));
        return;
    }

    settings.setValue(
        "Image/LastDirectory",
        QFileInfo(path).absolutePath()
        );

    image = loaded;
    pieces.clear();
    pieceAnalyses.clear();
    matches.clear();
    matchingList->clear();
    matchingScene->clear();

    showImage();
    showAlignedPieces();

    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);

    statusBar()->showMessage(
        tr("Bild geladen: %1 × %2 Pixel")
            .arg(image.width())
            .arg(image.height())
        );
}

void MainWindow::detectPieces()
{
    if (image.isNull())
    {
        statusBar()->showMessage(tr("Bitte zuerst ein Bild öffnen."));
        return;
    }

    QElapsedTimer totalTimer;
    QElapsedTimer stepTimer;

    totalTimer.start();

    PuzzleDetector detector;

    stepTimer.start();
    pieces = detector.detect(image);
    const qint64 detectionTime = stepTimer.elapsed();

    stepTimer.restart();
    updatePieceAnalyses();
    const qint64 analysisTime = stepTimer.elapsed();

    stepTimer.restart();
    showImage();
    const qint64 imageTime = stepTimer.elapsed();

    stepTimer.restart();
    showAlignedPieces();
    const qint64 alignmentTime = stepTimer.elapsed();

    stepTimer.restart();
    updateMatching();
    const qint64 matchingTime = stepTimer.elapsed();

    const qint64 totalTime = totalTimer.elapsed();

    qInfo().noquote()
        << QStringLiteral(
               "JigsawFoundry: Erkennung=%1 ms, "
               "Kantenanalyse=%2 ms, "
               "Bild=%3 ms, "
               "Ausrichtung/Darstellung=%4 ms, "
               "Matching=%5 ms, "
               "Gesamt=%6 ms")
               .arg(detectionTime)
               .arg(analysisTime)
               .arg(imageTime)
               .arg(alignmentTime)
               .arg(matchingTime)
               .arg(totalTime);

    statusBar()->showMessage(
        tr("%1 Puzzleteile | Erkennung: %2 ms | "
           "Kantenanalyse: %3 ms | "
           "Darstellung: %4 ms | "
           "Matching: %5 ms | Gesamt: %6 ms")
            .arg(pieces.size())
            .arg(detectionTime)
            .arg(analysisTime)
            .arg(imageTime + alignmentTime)
            .arg(matchingTime)
            .arg(totalTime)
        );
}

void MainWindow::showImage()
{
    scene->clear();
    firstMatchingHighlight = nullptr;
    secondMatchingHighlight = nullptr;

    if (image.isNull())
        return;

    QImage display = image.convertToFormat(QImage::Format_ARGB32);

    QPainter painter(&display);
    painter.setRenderHint(QPainter::Antialiasing);

    const double factor =
        static_cast<double>(image.width()) / 1536.0;

    const double lineWidth = std::max(1.5, 3.0 * factor);
    const int radius = std::max(5, static_cast<int>(17 * factor));

    const QTransform identity;

    for (int pieceIndex = 0; pieceIndex < pieces.size(); ++pieceIndex)
    {
        const PuzzlePiece& piece = pieces[pieceIndex];
        const PuzzleContourEdges& edges = pieceAnalyses[pieceIndex];

        if (edges.valid)
        {
            drawContourEdges(
                painter,
                edges,
                identity,
                lineWidth,
                false
                );
            drawEdgeLabels(
                painter,
                edges,
                identity,
                std::max(10.0, 16.0 * factor)
                );
        }
        else
        {
            painter.setPen(QPen(QColor("#808080"), lineWidth));
            painter.setBrush(Qt::NoBrush);
            painter.drawPolygon(piece.contour);
        }

        painter.setPen(Qt::white);
        painter.setBrush(QColor(0, 0, 0, 210));

        painter.drawEllipse(piece.center, radius, radius);

        QFont font = painter.font();
        font.setPixelSize(std::max(8, static_cast<int>(19 * factor)));
        font.setBold(true);
        painter.setFont(font);

        painter.drawText(
            QRectF(
                piece.center.x() - radius,
                piece.center.y() - radius,
                radius * 2,
                radius * 2
                ),
            Qt::AlignCenter,
            QString::number(piece.id)
            );
    }

    painter.end();

    scene->addPixmap(QPixmap::fromImage(display));
    scene->setSceneRect(image.rect());

    if (matchingList != nullptr)
        highlightMatchingPieces(matchingList->currentRow());
}

void MainWindow::showAlignedPieces()
{
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

void MainWindow::updatePieceAnalyses()
{
    pieceAnalyses.clear();
    pieceRotations.clear();
    pieceBaseLines.clear();

    pieceAnalyses.reserve(pieces.size());
    pieceRotations.reserve(pieces.size());
    pieceBaseLines.reserve(pieces.size());

    PuzzleEdgeDetector edgeDetector;

    for (const PuzzlePiece& piece : pieces)
    {
        const double rotation =
            edgeDetector.findRotation(piece.contour);

        const PuzzleBaseLines lines =
            edgeDetector.findBaseLines(piece.contour, rotation);

        const PuzzleContourEdges edges =
            edgeDetector.findContourEdges(
                piece.contour,
                rotation,
                lines
                );

        pieceRotations.append(rotation);
        pieceBaseLines.append(lines);
        pieceAnalyses.append(edges);
    }
}

void MainWindow::updateMatching()
{
    matchingList->clear();
    matchingScene->clear();
    pieceRotations.clear();
    pieceBaseLines.clear();
    matches.clear();

    if (pieces.isEmpty())
        return;

    PuzzleEdgeMatcher matcher;
    matches = matcher.findMatches(pieces, pieceAnalyses, image);

    const int maximumResults = std::min(200, static_cast<int>(matches.size()));

    for (int index = 0; index < maximumResults; ++index)
    {
        const PuzzleEdgeMatch& match = matches[index];
        const PuzzlePiece& first = pieces[match.firstPieceIndex];
        const PuzzlePiece& second = pieces[match.secondPieceIndex];

        const QString prefix = match.mutualBestMatch
                                   ? tr("★ ")
                                   : QString();

        const QString text =
            prefix +
            tr("Teil %1 / Kante %2  ↔  Teil %3 / Kante %4  |  G: %5  F alt: %6  F neu: %7  Gesamt: %8")
                .arg(first.id)
                .arg(match.firstEdgeIndex + 1)
                .arg(second.id)
                .arg(match.secondEdgeIndex + 1)
                .arg(match.geometryScore, 0, 'f', 2)
                .arg(match.previousColorScore, 0, 'f', 2)
                .arg(match.colorScore, 0, 'f', 2)
                .arg(match.score, 0, 'f', 2);

        QListWidgetItem* item = new QListWidgetItem(text);

        if (match.mutualBestMatch)
        {
            item->setForeground(QColor("#FFD600"));

            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }

        matchingList->addItem(item);
    }

    if (maximumResults > 0)
        matchingList->setCurrentRow(0);

    statusBar()->showMessage(tr("%1 Puzzleteile, %2 mögliche Kantenpaare")
                                 .arg(pieces.size()).arg(matches.size()));
}

void MainWindow::showMatchingPreview(int matchIndex)
{
    matchingScene->clear();

    if (matchIndex < 0 || matchIndex >= matchingList->count() || matchIndex >= matches.size())
        return;

    const PuzzleEdgeMatch& match = matches[matchIndex];
    const PuzzlePiece& first = pieces[match.firstPieceIndex];
    const PuzzlePiece& second = pieces[match.secondPieceIndex];
    const PuzzleContourEdges& firstAnalysis = pieceAnalyses[match.firstPieceIndex];
    const PuzzleContourEdges& secondAnalysis = pieceAnalyses[match.secondPieceIndex];
    const QPolygonF& firstEdge = firstAnalysis.edges[match.firstEdgeIndex];
    const QPolygonF& secondEdge = secondAnalysis.edges[match.secondEdgeIndex];

    if (firstEdge.size() < 2 || secondEdge.size() < 2)
        return;

    const QImage firstImage = extractPiece(image, first);
    const QImage secondImage = extractPiece(image, second);

    if (firstImage.isNull() || secondImage.isNull())
        return;

    const QPointF firstStart = firstEdge.first();
    const QPointF firstEnd = firstEdge.last();
    const QPointF secondStart = secondEdge.first();
    const QPointF secondEnd = secondEdge.last();

    const double firstAngle = std::atan2(firstStart.y() - firstEnd.y(), firstStart.x() - firstEnd.x());
    const double secondAngle = std::atan2(secondEnd.y() - secondStart.y(), secondEnd.x() - secondStart.x());

    QTransform secondRotation;
    secondRotation.rotateRadians(firstAngle - secondAngle);

    const QPointF firstOffset = -first.center;
    const QPointF secondOffset = firstEnd + firstOffset - secondRotation.map(secondStart);

    const QRect firstBounds = first.contour.boundingRect().adjusted(-2, -2, 2, 2).toAlignedRect().intersected(image.rect());
    const QRect secondBounds = second.contour.boundingRect().adjusted(-2, -2, 2, 2).toAlignedRect().intersected(image.rect());

    QGraphicsPixmapItem* firstItem = matchingScene->addPixmap(QPixmap::fromImage(firstImage));
    firstItem->setPos(QPointF(firstBounds.topLeft()) + firstOffset);

    QGraphicsPixmapItem* secondItem = matchingScene->addPixmap(QPixmap::fromImage(secondImage));
    secondItem->setTransform(secondRotation);
    secondItem->setPos(secondRotation.map(QPointF(secondBounds.topLeft())) + secondOffset);

    QPolygonF firstPreviewEdge;
    QPolygonF secondPreviewEdge;

    for (const QPointF& point : firstEdge)
        firstPreviewEdge.append(point + firstOffset);

    for (const QPointF& point : secondEdge)
        secondPreviewEdge.append(secondRotation.map(point) + secondOffset);

    QGraphicsPathItem* firstLine = matchingScene->addPath(QPainterPath(), QPen(QColor("#00E5FF"), 3.0));
    QPainterPath firstPath;
    firstPath.moveTo(firstPreviewEdge.first());
    for (int index = 1; index < firstPreviewEdge.size(); ++index)
        firstPath.lineTo(firstPreviewEdge[index]);
    firstLine->setPath(firstPath);
    firstLine->setZValue(10.0);

    QGraphicsPathItem* secondLine = matchingScene->addPath(QPainterPath(), QPen(QColor("#FF9100"), 3.0));
    QPainterPath secondPath;
    secondPath.moveTo(secondPreviewEdge.first());
    for (int index = 1; index < secondPreviewEdge.size(); ++index)
        secondPath.lineTo(secondPreviewEdge[index]);
    secondLine->setPath(secondPath);
    secondLine->setZValue(10.0);

    QGraphicsTextItem* firstLabel = matchingScene->addText(tr("Teil %1 – Nase").arg(first.id));
    firstLabel->setDefaultTextColor(QColor("#00E5FF"));
    firstLabel->setPos(first.center + firstOffset);
    firstLabel->setZValue(20.0);

    QGraphicsTextItem* secondLabel = matchingScene->addText(tr("Teil %1 – Einbuchtung").arg(second.id));
    secondLabel->setDefaultTextColor(QColor("#FF9100"));
    secondLabel->setPos(secondRotation.map(second.center) + secondOffset);
    secondLabel->setZValue(20.0);

    const QRectF bounds = matchingScene->itemsBoundingRect().adjusted(-30.0, -30.0, 30.0, 30.0);
    matchingScene->setSceneRect(bounds);
    matchingView->fitInView(bounds, Qt::KeepAspectRatio);
}

void MainWindow::highlightMatchingPieces(int matchIndex)
{
    if (firstMatchingHighlight != nullptr)
    {
        scene->removeItem(firstMatchingHighlight);
        delete firstMatchingHighlight;
        firstMatchingHighlight = nullptr;
    }

    if (secondMatchingHighlight != nullptr)
    {
        scene->removeItem(secondMatchingHighlight);
        delete secondMatchingHighlight;
        secondMatchingHighlight = nullptr;
    }

    if (matchIndex < 0 || matchIndex >= matches.size())
        return;

    const PuzzleEdgeMatch& match = matches[matchIndex];

    if (match.firstPieceIndex < 0 || match.firstPieceIndex >= pieces.size() ||
        match.secondPieceIndex < 0 || match.secondPieceIndex >= pieces.size())
        return;

    const PuzzlePiece& first = pieces[match.firstPieceIndex];
    const PuzzlePiece& second = pieces[match.secondPieceIndex];

    const double factor = static_cast<double>(image.width()) / 1536.0;
    const double lineWidth = std::max(5.0, 6.0 * factor);

    firstMatchingHighlight = scene->addPolygon(
        first.contour,
        QPen(QColor("#00FFFF"), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
        QColor(0, 255, 255, 100)
        );

    secondMatchingHighlight = scene->addPolygon(
        second.contour,
        QPen(QColor("#FF9800"), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin),
        QColor(255, 152, 0, 100)
        );

    firstMatchingHighlight->setZValue(10.0);
    secondMatchingHighlight->setZValue(10.0);
}
