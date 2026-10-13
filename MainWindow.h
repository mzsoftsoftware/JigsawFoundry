#pragma once

#include "PuzzleDetector.h"
#include "PuzzleEdgeMatcher.h"

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

    void updatePieceAnalyses();
    void updateImageList();
    void updateMatching();

    void showImage();
    void showSecondImage();
    void showAlignedPieces();
    void showMatchingPreview(int matchIndex);
    void highlightMatchingPieces(int matchIndex);

    void restoreLayout();
    void saveLayout();

    void updateOriginalImages();

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

    QString imagePath;
    QImage image;

    QVector<PuzzlePiece> pieces;
    QVector<PuzzleContourEdges> pieceAnalyses;
    QVector<double> pieceRotations;
    QVector<PuzzleBaseLines> pieceBaseLines;
    QVector<PuzzleEdgeMatch> matches;

    QGraphicsPolygonItem* firstMatchingHighlight = nullptr;
    QGraphicsPolygonItem* secondMatchingHighlight = nullptr;

    QPixmap originalPixmap;

    QGraphicsPixmapItem* firstImageItem = nullptr;
    QGraphicsPixmapItem* secondImageItem = nullptr;


    QVector<QImage> pieceImageCache;

    void updatePieceImageCache();
};
