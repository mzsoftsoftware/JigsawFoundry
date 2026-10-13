#include "PuzzleEdgeMatcher.h"

#include <QColor>
#include <QPointF>
#include <QHash>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr int SampleCount = 64;
constexpr double ColorWeight = 0.35;

struct EdgeProfile
{
    QVector<double> values;
    double length = 0.0;
    bool valid = false;
};

struct LabColor
{
    double l = 0.0;
    double a = 0.0;
    double b = 0.0;
};

struct ColorProfile
{
    QVector<LabColor> values;
    QVector<bool> valid;
};

double distance(const QPointF& first, const QPointF& second)
{
    return std::hypot(second.x() - first.x(), second.y() - first.y());
}

EdgeProfile createProfile(const QPolygonF& contour)
{
    EdgeProfile result;
    if (contour.size() < 3)
        return result;

    const QPointF start = contour.first();
    const QPointF end = contour.last();
    const double dx = end.x() - start.x();
    const double dy = end.y() - start.y();
    result.length = std::hypot(dx, dy);
    if (result.length < 5.0)
        return result;

    struct Sample
    {
        double position;
        double deviation;
    };

    QVector<Sample> samples;
    samples.reserve(contour.size());
    const double inverseLengthSquared = 1.0 / (result.length * result.length);

    for (const QPointF& point : contour)
    {
        const double px = point.x() - start.x();
        const double py = point.y() - start.y();
        samples.append({(px * dx + py * dy) * inverseLengthSquared, (dx * py - dy * px) * inverseLengthSquared});
    }

    std::sort(samples.begin(), samples.end(), [](const Sample& first, const Sample& second) { return first.position < second.position; });

    int upper = 0;
    for (int index = 0; index < SampleCount; ++index)
    {
        const double position = static_cast<double>(index) / (SampleCount - 1);
        while (upper < samples.size() && samples[upper].position < position)
            ++upper;

        if (upper == 0)
            result.values.append(samples.first().deviation);
        else if (upper == samples.size())
            result.values.append(samples.last().deviation);
        else
        {
            const Sample& first = samples[upper - 1];
            const Sample& second = samples[upper];
            const double interval = second.position - first.position;
            const double fraction = interval > 1e-9 ? (position - first.position) / interval : 0.0;
            result.values.append(first.deviation + (second.deviation - first.deviation) * fraction);
        }
    }

    result.valid = true;
    return result;
}

double compareProfiles(const EdgeProfile& first, const EdgeProfile& second)
{
    if (!first.valid || !second.valid)
        return std::numeric_limits<double>::infinity();

    const double averageLength = (first.length + second.length) * 0.5;
    const double lengthDifference = std::abs(first.length - second.length) / averageLength;

    if (lengthDifference > 0.20)
        return std::numeric_limits<double>::infinity();

    double squaredError = 0.0;

    for (int index = 0; index < SampleCount; ++index)
    {
        const double difference =
            first.values[index] + second.values[SampleCount - 1 - index];

        squaredError += difference * difference;
    }

    return std::sqrt(squaredError / SampleCount) * 100.0
           + lengthDifference * 100.0;
}

QPointF pointAtFraction(const QPolygonF& contour, double fraction)
{
    QVector<double> lengths;
    lengths.reserve(contour.size());
    lengths.append(0.0);
    for (int index = 1; index < contour.size(); ++index)
        lengths.append(lengths.last() + distance(contour[index - 1], contour[index]));

    const double target = lengths.last() * fraction;
    for (int index = 1; index < lengths.size(); ++index)
    {
        if (lengths[index] >= target)
        {
            const double segmentLength = lengths[index] - lengths[index - 1];
            const double t = segmentLength > 1e-9 ? (target - lengths[index - 1]) / segmentLength : 0.0;
            return contour[index - 1] + (contour[index] - contour[index - 1]) * t;
        }
    }
    return contour.last();
}

double linearRgb(double value)
{
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

double labFunction(double value)
{
    return value > 0.008856451679 ? std::cbrt(value) : (903.296296296 * value + 16.0) / 116.0;
}

LabColor toLab(const QColor& color)
{
    const double r = linearRgb(color.redF());
    const double g = linearRgb(color.greenF());
    const double b = linearRgb(color.blueF());

    const double x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047;
    const double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
    const double z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883;

    const double fx = labFunction(x);
    const double fy = labFunction(y);
    const double fz = labFunction(z);
    return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

bool sampleLab(const QImage& image, const QPointF& point, LabColor& color)
{
    const int x = static_cast<int>(std::lround(point.x()));
    const int y = static_cast<int>(std::lround(point.y()));
    if (x < 0 || y < 0 || x >= image.width() || y >= image.height())
        return false;
    color = toLab(QColor::fromRgb(image.pixel(x, y)));
    return true;
}

ColorProfile createColorProfile(const QImage& image, const QPolygonF& edge, const QPointF& center)
{
    ColorProfile result;
    if (edge.size() < 2)
        return result;

    const QPointF start = edge.first();
    const QPointF end = edge.last();
    const QPointF chord = end - start;
    const double chordLength = std::hypot(chord.x(), chord.y());
    if (chordLength < 5.0)
        return result;

    const double cross = chord.x() * (center.y() - start.y()) - chord.y() * (center.x() - start.x());
    const double sign = cross >= 0.0 ? 1.0 : -1.0;
    const QPointF inward(-chord.y() * sign / chordLength, chord.x() * sign / chordLength);

    result.values.reserve(SampleCount);
    result.valid.reserve(SampleCount);

    for (int index = 0; index < SampleCount; ++index)
    {
        const double fraction = static_cast<double>(index) / (SampleCount - 1);
        LabColor sum;
        int count = 0;

        if (fraction >= 0.08 && fraction <= 0.92)
        {
            const QPointF contourPoint = pointAtFraction(edge, fraction);
            for (const double offset : {6.0, 10.0, 14.0})
            {
                LabColor color;
                if (!sampleLab(image, contourPoint + inward * offset, color))
                    continue;
                sum.l += color.l;
                sum.a += color.a;
                sum.b += color.b;
                ++count;
            }
        }

        if (count > 0)
        {
            result.values.append({sum.l / count, sum.a / count, sum.b / count});
            result.valid.append(true);
        }
        else
        {
            result.values.append(LabColor{});
            result.valid.append(false);
        }
    }
    return result;
}

double compareColors(const ColorProfile& outward, const ColorProfile& inward)
{
    double error = 0.0;
    int count = 0;
    for (int index = 0; index < SampleCount; ++index)
    {
        const int opposite = SampleCount - 1 - index;
        if (!outward.valid[index] || !inward.valid[opposite])
            continue;

        const LabColor& first = outward.values[index];
        const LabColor& second = inward.values[opposite];
        const double dl = first.l - second.l;
        const double da = first.a - second.a;
        const double db = first.b - second.b;
        error += std::sqrt(dl * dl + da * da + db * db);
        ++count;
    }
    return count >= SampleCount / 2 ? error / count : std::numeric_limits<double>::infinity();
}


}


QVector<PuzzleEdgeMatch> PuzzleEdgeMatcher::findMatches(
    const QVector<PuzzlePiece>& pieces,
    const QVector<PuzzleContourEdges>& analyses,
    const QImage& image) const
{
    QVector<PuzzleEdgeMatch> matches;

    if (pieces.size() != analyses.size() || image.isNull())
        return matches;

    struct CandidateEdge
    {
        int pieceIndex = -1;
        int edgeIndex = -1;
        EdgeProfile geometry;
        ColorProfile colors;
    };

    QVector<CandidateEdge> outwardEdges;
    QVector<CandidateEdge> inwardEdges;

    for (int pieceIndex = 0; pieceIndex < pieces.size(); ++pieceIndex)
    {
        const PuzzleContourEdges& analysis = analyses[pieceIndex];

        if (!analysis.usableForMatching)
            continue;

        const int count = std::min(
            static_cast<int>(analysis.edges.size()),
            static_cast<int>(analysis.classifications.size())
            );

        for (int edgeIndex = 0; edgeIndex < count; ++edgeIndex)
        {
            const PuzzleEdgeType type =
                analysis.classifications[edgeIndex].type;

            if (type != PuzzleEdgeType::Outward &&
                type != PuzzleEdgeType::Inward)
                continue;

            const QPolygonF& edge = analysis.edges[edgeIndex];

            CandidateEdge candidate;
            candidate.pieceIndex = pieceIndex;
            candidate.edgeIndex = edgeIndex;
            candidate.geometry = createProfile(edge);

            if (!candidate.geometry.valid)
                continue;

            candidate.colors = createColorProfile(
                image,
                edge,
                pieces[pieceIndex].center
                );

            if (type == PuzzleEdgeType::Outward)
                outwardEdges.append(candidate);
            else
                inwardEdges.append(candidate);
        }
    }

    for (const CandidateEdge& first : outwardEdges)
    {
        for (const CandidateEdge& second : inwardEdges)
        {
            if (first.pieceIndex == second.pieceIndex)
                continue;

            const double geometry =
                compareProfiles(first.geometry, second.geometry);

            if (!std::isfinite(geometry) || geometry > 10.0)
                continue;

            const double color =
                compareColors(first.colors, second.colors);

            if (!std::isfinite(color) || color > 25.0)
                continue;

            const double score = geometry + ColorWeight * color;

            if (score > 15.0)
                continue;

            PuzzleEdgeMatch match;
            match.firstPieceIndex = first.pieceIndex;
            match.firstEdgeIndex = first.edgeIndex;
            match.secondPieceIndex = second.pieceIndex;
            match.secondEdgeIndex = second.edgeIndex;
            match.geometryScore = geometry;
            match.colorScore = color;
            match.previousColorScore =
                std::numeric_limits<double>::infinity();
            match.score = score;

            matches.append(match);
        }
    }

    std::sort(
        matches.begin(),
        matches.end(),
        [](const PuzzleEdgeMatch& first, const PuzzleEdgeMatch& second)
        {
            return first.score < second.score;
        }
        );

    // Die Treffer sind bereits aufsteigend nach Score sortiert.
    // Die erste gefundene Verwendung einer Kante ist deshalb
    // automatisch deren bester Treffer.

    QHash<quint64, double> bestOutward;
    QHash<quint64, double> bestInward;

    for (PuzzleEdgeMatch& match : matches)
    {
        const quint64 outwardKey =
            (static_cast<quint64>(match.firstPieceIndex) << 32) |
            static_cast<quint32>(match.firstEdgeIndex);

        const quint64 inwardKey =
            (static_cast<quint64>(match.secondPieceIndex) << 32) |
            static_cast<quint32>(match.secondEdgeIndex);

        const bool firstIsBest =
            !bestOutward.contains(outwardKey) ||
            bestOutward.value(outwardKey) >= match.score;

        const bool secondIsBest =
            !bestInward.contains(inwardKey) ||
            bestInward.value(inwardKey) >= match.score;

        match.mutualBestMatch = firstIsBest && secondIsBest;

        if (!bestOutward.contains(outwardKey))
            bestOutward.insert(outwardKey, match.score);

        if (!bestInward.contains(inwardKey))
            bestInward.insert(inwardKey, match.score);
    }

    std::stable_sort(
        matches.begin(),
        matches.end(),
        [](const PuzzleEdgeMatch& first, const PuzzleEdgeMatch& second)
        {
            if (first.mutualBestMatch != second.mutualBestMatch)
                return first.mutualBestMatch;

            return first.score < second.score;
        }
        );

    return matches;
}
