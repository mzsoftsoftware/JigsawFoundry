#pragma once

#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QVector>

enum class PuzzleEdgeType
{
    Unknown,
    Flat,
    Inward,
    Outward
};

struct PuzzleEdge
{
    PuzzleEdgeType type = PuzzleEdgeType::Unknown;
    QPolygonF contour;
    QPointF start;
    QPointF end;
};

struct PuzzlePiece
{
    int id = 0;
    QPolygonF contour;
    QPointF center;
    QVector<QPointF> corners;
    QVector<PuzzleEdge> edges;
    QPolygonF topContour;
    QVector<bool> topContourValid;
};

class PuzzleDetector
{
public:
    QVector<PuzzlePiece> detect(const QImage& image) const;
    void detectTopContours(
        const QImage& image,
        QVector<PuzzlePiece>& pieces) const;
};
