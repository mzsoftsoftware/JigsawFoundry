#pragma once

#include "PuzzleDetector.h"
#include "PuzzleEdgeMatcher.h"
#include "PuzzleImage.h"

#include <QImage>
#include <QMainWindow>
#include <QVector>

class QListWidget;
class QGraphicsScene;
class QGraphicsView;
class QGraphicsPolygonItem;
class QSplitter;
class QCloseEvent;
class QGraphicsPixmapItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void openImage();
    void detectPieces();

    void updateImageList();
    void updateMatching();

    void showAlignedPieces(int imageIndex);
    void showMatchingPreview(int matchIndex);
    void highlightMatchingPieces(int matchIndex);

    void restoreLayout();
    void saveLayout();

    void updateOriginalHighlight(
        QGraphicsPolygonItem* highlight,
        const PuzzlePiece* piece
        );

    void fitOriginalImages();

    QSplitter* mainSplitter = nullptr;
    QSplitter* leftSplitter = nullptr;
    QSplitter* rightSplitter = nullptr;

    QListWidget* imageList = nullptr;
    QListWidget* matchingList = nullptr;

    QGraphicsScene* scene = nullptr;
    QGraphicsView* view = nullptr;

    QGraphicsScene* secondScene = nullptr;
    QGraphicsView* secondView = nullptr;

    QGraphicsScene* matchingScene = nullptr;
    QGraphicsView* matchingView = nullptr;

    // Vorhandene Diagnoseansicht bleibt erhalten.
    QGraphicsScene* alignedScene = nullptr;
    QGraphicsView* alignedView = nullptr;

    QGraphicsPolygonItem* firstMatchingHighlight = nullptr;
    QGraphicsPolygonItem* secondMatchingHighlight = nullptr;

    QGraphicsPixmapItem* firstImageItem = nullptr;
    QGraphicsPixmapItem* secondImageItem = nullptr;

    QVector<PuzzleImage> images;
    QVector<PuzzleEdgeMatch> matches;

    int displayedFirstImageIndex = -1;
    int displayedSecondImageIndex = -1;

    void analyzeImage(PuzzleImage& puzzleImage);

    void displayOriginalImage(
        int imageIndex,
        QGraphicsScene* targetScene,
        QGraphicsPixmapItem*& imageItem,
        QGraphicsPolygonItem*& highlight,
        const QColor& highlightColor
        );
};
