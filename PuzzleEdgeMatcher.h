#pragma once

#include "PuzzleImage.h"

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
    double contourScore = 0.0;
    double colorScore = 0.0;
    double previousColorScore = 0.0;
    double score = 0.0;

    bool mutualBestMatch = false;
};

class PuzzleEdgeMatcher
{
public:
    void prepareImage(PuzzleImage& puzzleImage) const;

    QVector<PuzzleEdgeMatch> findMatches(
        const QVector<PuzzleImage>& images
        ) const;

    void calibrateImages(QVector<PuzzleImage>& images) const;
};
