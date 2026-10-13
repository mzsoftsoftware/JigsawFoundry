#pragma once

#include "PuzzleDetector.h"
#include "PuzzleEdgeDetector.h"

#include <QImage>
#include <QPixmap>
#include <QString>
#include <QVector>

struct PuzzleImage
{
    QString filePath;

    QImage image;
    QPixmap pixmap;
    QPixmap thumbnail;

    QVector<PuzzlePiece> pieces;
    QVector<PuzzleContourEdges> analyses;
    QVector<double> rotations;
    QVector<PuzzleBaseLines> baseLines;

    QVector<QImage> extractedPieces;
    QVector<QPixmap> pieceThumbnails;

    int usablePieces = 0;
};
