#pragma once

#include "PuzzleDetector.h"

#include <QPointF>
#include <QPolygonF>
#include <QVector>

struct PuzzleEdgeClassification
{
    PuzzleEdgeType type = PuzzleEdgeType::Unknown;

    double maximumDeviation = 0.0;
    double averageDeviation = 0.0;
    double relativeDepth = 0.0;
};

struct PuzzleContourEdges
{
    QVector<int> cornerIndices;
    QVector<QPointF> corners;
    QVector<QPolygonF> edges;
    QVector<PuzzleEdgeClassification> classifications;

    bool valid = false;
    bool usableForMatching = false;
};

struct PuzzleBaseLines
{
    double left = 0.0;
    double right = 0.0;
    double top = 0.0;
    double bottom = 0.0;

    bool valid = false;
};

class PuzzleEdgeDetector
{
public:
    double findRotation(const QPolygonF& contour) const;

    PuzzleBaseLines findBaseLines(
        const QPolygonF& contour,
        double rotationDegrees
        ) const;

    PuzzleContourEdges findContourEdges(
        const QPolygonF& contour,
        double rotationDegrees,
        const PuzzleBaseLines& lines
        ) const;

    PuzzleEdgeClassification classifyEdge(
        const QPolygonF& edge,
        const QPointF& pieceCenter
        ) const;
};
