#include "PuzzleEdgeDetector.h"

#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{

constexpr double Pi = 3.14159265358979323846;

double normalizeAngle(double angle)
{
    while (angle < 0.0)
        angle += Pi / 2.0;

    while (angle >= Pi / 2.0)
        angle -= Pi / 2.0;

    return angle;
}

double angleDifference(double a, double b)
{
    const double difference = std::abs(a - b);
    return std::min(difference, Pi / 2.0 - difference);
}

struct Segment
{
    double angle = 0.0;
    double length = 0.0;
};

std::vector<Segment> findSegments(const QPolygonF& contour)
{
    std::vector<Segment> segments;

    const int count = contour.size();

    if (count < 40)
        return segments;

    const int step = std::max(5, count / 80);

    for (int index = 0; index < count; index += step)
    {
        const QPointF& a = contour[index];
        const QPointF& b = contour[(index + step) % count];

        const double dx = b.x() - a.x();
        const double dy = b.y() - a.y();

        const double length = std::sqrt(dx * dx + dy * dy);

        if (length < 5.0)
            continue;

        Segment segment;
        segment.angle = normalizeAngle(std::atan2(dy, dx));
        segment.length = length;

        segments.push_back(segment);
    }

    return segments;
}

double alignmentScore(
    const std::vector<Segment>& segments,
    double angle)
{
    double score = 0.0;

    for (const Segment& segment : segments)
    {
        const double difference =
            angleDifference(segment.angle, angle);

        score += segment.length * std::cos(difference * 4.0);
    }

    return score;
}

QPointF rotatePoint(const QPointF& point, double angle)
{
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);

    return QPointF(
        point.x() * cosine - point.y() * sine,
        point.x() * sine + point.y() * cosine
        );
}

struct LineCandidate
{
    double position = 0.0;
    double weight = 0.0;
};

double weightedMedian(std::vector<LineCandidate> candidates)
{
    if (candidates.empty())
        return 0.0;

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const LineCandidate& a, const LineCandidate& b)
        {
            return a.position < b.position;
        }
        );

    double totalWeight = 0.0;

    for (const LineCandidate& candidate : candidates)
        totalWeight += candidate.weight;

    double accumulatedWeight = 0.0;

    for (const LineCandidate& candidate : candidates)
    {
        accumulatedWeight += candidate.weight;

        if (accumulatedWeight >= totalWeight * 0.5)
            return candidate.position;
    }

    return candidates.back().position;
}

}

double PuzzleEdgeDetector::findRotation(const QPolygonF& contour) const
{
    const std::vector<Segment> segments = findSegments(contour);

    if (segments.empty())
        return 0.0;

    double bestAngle = 0.0;
    double bestScore = -std::numeric_limits<double>::infinity();

    for (int step = 0; step < 900; ++step)
    {
        const double angle = step * 0.1 * Pi / 180.0;
        const double score = alignmentScore(segments, angle);

        if (score > bestScore)
        {
            bestScore = score;
            bestAngle = angle;
        }
    }

    return -bestAngle * 180.0 / Pi;
}

PuzzleBaseLines PuzzleEdgeDetector::findBaseLines(
    const QPolygonF& contour,
    double rotationDegrees) const
{
    PuzzleBaseLines result;

    if (contour.size() < 40)
        return result;

    const double angle = rotationDegrees * Pi / 180.0;

    QPolygonF rotated;
    rotated.reserve(contour.size());

    for (const QPointF& point : contour)
        rotated.append(rotatePoint(point, angle));

    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    for (const QPointF& point : rotated)
    {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    const double width = maxX - minX;
    const double height = maxY - minY;

    if (width < 10.0 || height < 10.0)
        return result;

    std::array<std::vector<LineCandidate>, 4> candidates;

    const int count = rotated.size();
    const int step = std::max(5, count / 100);

    const double centerX = (minX + maxX) * 0.5;
    const double centerY = (minY + maxY) * 0.5;

    const double maxDeviation = 8.0 * Pi / 180.0;

    for (int index = 0; index < count; ++index)
    {
        const QPointF& a = rotated[index];
        const QPointF& b = rotated[(index + step) % count];

        const double dx = b.x() - a.x();
        const double dy = b.y() - a.y();

        const double length = std::sqrt(dx * dx + dy * dy);

        if (length < 5.0)
            continue;

        const QPointF midpoint = (a + b) * 0.5;

        if (std::abs(dy) <= length * std::sin(maxDeviation))
        {
            LineCandidate candidate;
            candidate.position = midpoint.y();
            candidate.weight = length;

            if (midpoint.y() < centerY)
                candidates[2].push_back(candidate);
            else
                candidates[3].push_back(candidate);
        }

        if (std::abs(dx) <= length * std::sin(maxDeviation))
        {
            LineCandidate candidate;
            candidate.position = midpoint.x();
            candidate.weight = length;

            if (midpoint.x() < centerX)
                candidates[0].push_back(candidate);
            else
                candidates[1].push_back(candidate);
        }
    }

    for (const std::vector<LineCandidate>& side : candidates)
    {
        if (side.empty())
            return result;
    }

    result.left = weightedMedian(candidates[0]);
    result.right = weightedMedian(candidates[1]);
    result.top = weightedMedian(candidates[2]);
    result.bottom = weightedMedian(candidates[3]);

    result.valid =
        result.right > result.left &&
        result.bottom > result.top;

    return result;
}

PuzzleContourEdges PuzzleEdgeDetector::findContourEdges(
    const QPolygonF& contour,
    double rotationDegrees,
    const PuzzleBaseLines& lines) const
{
    PuzzleContourEdges result;

    if (!lines.valid || contour.size() < 40)
        return result;

    const double angle = rotationDegrees * Pi / 180.0;

    QPolygonF rotated;
    rotated.reserve(contour.size());

    for (const QPointF& point : contour)
        rotated.append(rotatePoint(point, angle));

    const QPointF targets[4] = {
        QPointF(lines.left, lines.top),
        QPointF(lines.right, lines.top),
        QPointF(lines.right, lines.bottom),
        QPointF(lines.left, lines.bottom)
    };

    const double width = lines.right - lines.left;
    const double height = lines.bottom - lines.top;

    if (width <= 0.0 || height <= 0.0)
        return result;

    const double bodySize = std::min(width, height);

    // Search only near the theoretical corner.
    const double searchRadius = bodySize * 0.16;
    const double searchRadiusSquared = searchRadius * searchRadius;

    const int count = rotated.size();

    // Estimate the contour directions on both sides of a candidate.
    // This is intentionally wider than a single pixel step.
    const int tangentStep = std::max(3, std::min(15, count / 120));

    for (const QPointF& target : targets)
    {
        int bestIndex = -1;
        double bestScore = std::numeric_limits<double>::infinity();

        for (int index = 0; index < count; ++index)
        {
            const QPointF& point = rotated[index];

            const double dx = point.x() - target.x();
            const double dy = point.y() - target.y();

            const double distanceSquared = dx * dx + dy * dy;

            if (distanceSquared > searchRadiusSquared)
                continue;

            const QPointF& before =
                rotated[(index - tangentStep + count) % count];

            const QPointF& after =
                rotated[(index + tangentStep) % count];

            const QPointF incoming = point - before;
            const QPointF outgoing = after - point;

            const double incomingLength = std::hypot(
                incoming.x(), incoming.y()
                );

            const double outgoingLength = std::hypot(
                outgoing.x(), outgoing.y()
                );

            if (incomingLength < 1.0 || outgoingLength < 1.0)
                continue;

            // A real corner normally joins one approximately
            // horizontal and one approximately vertical direction.
            const double incomingHorizontal =
                std::abs(incoming.y()) / incomingLength;

            const double incomingVertical =
                std::abs(incoming.x()) / incomingLength;

            const double outgoingHorizontal =
                std::abs(outgoing.y()) / outgoingLength;

            const double outgoingVertical =
                std::abs(outgoing.x()) / outgoingLength;

            const double horizontalThenVertical =
                incomingHorizontal + outgoingVertical;

            const double verticalThenHorizontal =
                incomingVertical + outgoingHorizontal;

            const double directionError = std::min(
                horizontalThenVertical,
                verticalThenHorizontal
                );

            const double normalizedDistance =
                std::sqrt(distanceSquared) / searchRadius;

            // Distance has priority, but a plausible change
            // of direction improves the candidate's score.
            const double score =
                normalizedDistance * 0.65 +
                directionError * 0.35;

            if (score < bestScore)
            {
                bestScore = score;
                bestIndex = index;
            }
        }

        if (bestIndex < 0)
            return result;

        result.cornerIndices.append(bestIndex);
    }

    std::sort(
        result.cornerIndices.begin(),
        result.cornerIndices.end()
        );

    // All four corners must refer to different contour positions.
    for (int index = 1; index < 4; ++index)
    {
        if (result.cornerIndices[index] ==
            result.cornerIndices[index - 1])
        {
            result.cornerIndices.clear();
            return result;
        }
    }

    // Prevent two corners from collapsing onto the same short
    // contour section.
    const int minimumGap = std::max(3, count / 20);

    for (int side = 0; side < 4; ++side)
    {
        const int startIndex = result.cornerIndices[side];
        const int endIndex = result.cornerIndices[(side + 1) % 4];

        const int gap = (endIndex - startIndex + count) % count;

        if (gap < minimumGap)
        {
            result.cornerIndices.clear();
            return result;
        }
    }

    // Store the real corners in the original image coordinates.
    for (int index : result.cornerIndices)
        result.corners.append(contour[index]);

    // Split the original contour into four connected sections.
    for (int side = 0; side < 4; ++side)
    {
        const int startIndex = result.cornerIndices[side];
        const int endIndex = result.cornerIndices[(side + 1) % 4];

        QPolygonF edge;

        int index = startIndex;

        while (true)
        {
            edge.append(contour[index]);

            if (index == endIndex)
                break;

            index = (index + 1) % count;
        }

        result.edges.append(edge);
    }

    result.valid =
        result.corners.size() == 4 &&
        result.edges.size() == 4;

    if (result.valid)
    {
        // Use the center of the four theoretical base lines.
        // This is more stable than the center of the contour,
        // which can be shifted by puzzle tabs.
        const QPointF rotatedCenter(
            (lines.left + lines.right) * 0.5,
            (lines.top + lines.bottom) * 0.5
            );

        const QPointF pieceCenter =
            rotatePoint(rotatedCenter, -angle);

        for (const QPolygonF& edge : result.edges)
        {
            result.classifications.append(
                classifyEdge(edge, pieceCenter)
                );
        }

        result.usableForMatching = result.valid;

        for (const PuzzleEdgeClassification& classification : result.classifications)
        {
            if (classification.type == PuzzleEdgeType::Unknown)
            {
                result.usableForMatching = false;
                break;
            }
        }
    }

    return result;
}

PuzzleEdgeClassification PuzzleEdgeDetector::classifyEdge(
    const QPolygonF& edge,
    const QPointF& pieceCenter) const
{
    PuzzleEdgeClassification result;

    if (edge.size() < 5)
        return result;

    const QPointF start = edge.first();
    const QPointF end = edge.last();

    const double dx = end.x() - start.x();
    const double dy = end.y() - start.y();

    const double length = std::hypot(dx, dy);

    if (length < 5.0)
        return result;

    const double centerSide =
        dx * (pieceCenter.y() - start.y()) -
        dy * (pieceCenter.x() - start.x());

    if (std::abs(centerSide) < 0.001)
        return result;

    // Signed distance from the line joining the two real corners.
    // Positive values indicate the same side as the piece center.
    const double sideSign = centerSide > 0.0 ? 1.0 : -1.0;

    QVector<double> deviations;
    deviations.reserve(edge.size());

    double maximumInward = 0.0;
    double maximumOutward = 0.0;

    double inwardArea = 0.0;
    double outwardArea = 0.0;

    double totalLength = 0.0;

    for (int index = 0; index < edge.size(); ++index)
    {
        const QPointF& point = edge[index];

        const double signedDistance =
            (
                dx * (point.y() - start.y()) -
                dy * (point.x() - start.x())
                ) / length * sideSign;

        deviations.append(signedDistance);

        maximumInward = std::max(maximumInward, signedDistance);
        maximumOutward = std::max(maximumOutward, -signedDistance);

        if (index == 0)
            continue;

        const QPointF& previous = edge[index - 1];

        const double segmentLength = std::hypot(
            point.x() - previous.x(),
            point.y() - previous.y()
            );

        const double previousDeviation = deviations[index - 1];
        const double averageSegmentDeviation =
            (previousDeviation + signedDistance) * 0.5;

        if (averageSegmentDeviation > 0.0)
            inwardArea += averageSegmentDeviation * segmentLength;
        else
            outwardArea -= averageSegmentDeviation * segmentLength;

        totalLength += segmentLength;
    }

    if (totalLength < 1.0)
        return result;

    result.maximumDeviation = std::max(
        maximumInward,
        maximumOutward
        );

    result.averageDeviation =
        (inwardArea - outwardArea) / totalLength;

    result.relativeDepth = result.maximumDeviation / length;

    // Small contour irregularities should not count as tabs or sockets.
    const double minimumDepth = std::max(3.0, length * 0.055);

    if (result.maximumDeviation < minimumDepth)
    {
        result.type = PuzzleEdgeType::Flat;
        return result;
    }

    // A tab or socket should occupy a meaningful part of the edge.
    // Evaluate the signed area to avoid reacting to single outliers.
    const double minimumArea = length * length * 0.012;

    const bool significantInward =
        maximumInward >= minimumDepth &&
        inwardArea >= minimumArea;

    const bool significantOutward =
        maximumOutward >= minimumDepth &&
        outwardArea >= minimumArea;

    if (significantInward && !significantOutward)
    {
        result.type = PuzzleEdgeType::Inward;
    }
    else if (significantOutward && !significantInward)
    {
        result.type = PuzzleEdgeType::Outward;
    }
    else if (!significantInward && !significantOutward)
    {
        result.type = PuzzleEdgeType::Flat;
    }
    else
    {
        // Both directions are significant.
        // Classify only when one direction clearly dominates.
        constexpr double dominanceFactor = 1.5;

        if (inwardArea > outwardArea * dominanceFactor)
        {
            result.type = PuzzleEdgeType::Inward;
        }
        else if (outwardArea > inwardArea * dominanceFactor)
        {
            result.type = PuzzleEdgeType::Outward;
        }
        else
        {
            result.type = PuzzleEdgeType::Unknown;
        }
    }

    return result;
}
