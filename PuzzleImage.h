#pragma once

#include "PuzzleDetector.h"
#include "PuzzleEdgeDetector.h"

#include <QImage>
#include <QPixmap>
#include <QString>
#include <QVector>


struct PuzzleEdgeGeometryProfile
{
    QVector<double> values;
    double length = 0.0;
    bool valid = false;
};

struct PuzzleLabColor
{
    double l = 0.0;
    double a = 0.0;
    double b = 0.0;
};

struct PuzzleEdgeColorProfile
{
    QVector<PuzzleLabColor> values;
    QVector<bool> valid;
};

struct PuzzleCachedEdge
{
    int pieceIndex = -1;
    int edgeIndex = -1;

    PuzzleEdgeType type = PuzzleEdgeType::Unknown;

    PuzzleEdgeGeometryProfile geometry;
    PuzzleEdgeColorProfile colors;
    QVector<QPointF> contour2D;
};

struct PuzzleImage
{
    QString filePath;

    QImage image;
    QPixmap pixmap;
    QPixmap thumbnail;

    double typicalEdgeLength = 0.0;
    double scaleFactor = 1.0;
    bool scaleValid = false;

    PuzzleLabColor tableColor;
    bool tableColorValid = false;

    QVector<PuzzlePiece> pieces;
    QVector<PuzzleContourEdges> analyses;
    QVector<double> rotations;
    QVector<PuzzleBaseLines> baseLines;

    QVector<QImage> extractedPieces;
    QVector<QPixmap> pieceThumbnails;

    int usablePieces = 0;

    QVector<PuzzleCachedEdge> cachedEdges;
};
