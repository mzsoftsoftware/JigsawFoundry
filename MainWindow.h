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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void openImage();
    void detectPieces();
    void showImage();
    void showAlignedPieces();

    QGraphicsScene* scene = nullptr;
    QGraphicsView* view = nullptr;

    QGraphicsScene* alignedScene = nullptr;
    QGraphicsView* alignedView = nullptr;

    QImage image;
    QVector<PuzzlePiece> pieces;

    void updateMatching();
    void showMatchingPreview(int matchIndex);

    QListWidget* matchingList = nullptr;

    QGraphicsScene* matchingScene = nullptr;
    QGraphicsView* matchingView = nullptr;

    QVector<PuzzleContourEdges> pieceAnalyses;
    QVector<PuzzleEdgeMatch> matches;

    void highlightMatchingPieces(int matchIndex);

    QGraphicsPolygonItem* firstMatchingHighlight = nullptr;
    QGraphicsPolygonItem* secondMatchingHighlight = nullptr;
};
