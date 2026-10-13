#pragma once

#include "PuzzleDetector.h"
#include "PuzzleEdgeDetector.h"

#include <QImage>
#include <QVector>

struct PuzzleEdgeMatch
{
    int firstImageIndex = -1;
    int firstPieceIndex = -1;
    int firstEdgeIndex = -1;

    int secondImageIndex = -1;
    int secondPieceIndex = -1;
    int secondEdgeIndex = -1;

    double geometryScore = 0.0;
    double colorScore = 0.0;
    double previousColorScore = 0.0;
    double score = 0.0;

    bool mutualBestMatch = false;
};

class PuzzleEdgeMatcher
{
public:
    QVector<PuzzleEdgeMatch> findMatches(const QVector<PuzzlePiece>& pieces, const QVector<PuzzleContourEdges>& analyses, const QImage& image) const;

private:
};
