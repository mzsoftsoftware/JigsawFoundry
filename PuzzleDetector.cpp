#include "PuzzleDetector.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>
#include <algorithm>
#include <cmath>
#include <vector>


QVector<PuzzlePiece> PuzzleDetector::detect(const QImage& image) const
{
    QVector<PuzzlePiece> pieces;
    if (image.isNull())
        return pieces;

    // QImage owns the buffer. Clone it so OpenCV works on independent data.
    QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgb(rgbImage.height(), rgbImage.width(), CV_8UC3,
                rgbImage.bits(), static_cast<size_t>(rgbImage.bytesPerLine()));
    cv::Mat lab;
    cv::cvtColor(rgb, lab, cv::COLOR_RGB2Lab);

    // Initial calibration for the blue pieces on the wooden table.
    // In OpenCV's 8-bit Lab, low b values favor blue over yellow wood.
    cv::Mat mask;
    cv::inRange(lab, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 140), mask);

    cv::Mat labels, stats, centers;
    const int componentCount = cv::connectedComponentsWithStats(
        mask, labels, stats, centers, 8, CV_32S);

    const double scale = static_cast<double>(image.width()) / 1536.0;
    const double minArea = 10000.0 * scale * scale;
    const double maxArea = 30000.0 * scale * scale;

    for (int component = 1; component < componentCount; ++component)
    {
        const int area = stats.at<int>(component, cv::CC_STAT_AREA);
        if (area < minArea || area > maxArea)
            continue;

        const int x = stats.at<int>(component, cv::CC_STAT_LEFT);
        const int y = stats.at<int>(component, cv::CC_STAT_TOP);
        const int width = stats.at<int>(component, cv::CC_STAT_WIDTH);
        const int height = stats.at<int>(component, cv::CC_STAT_HEIGHT);
        if (width < 10 || height < 10)
            continue;

        cv::Mat componentMask = (labels(cv::Rect(x, y, width, height)) == component);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(componentMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
        if (contours.empty())
            continue;

        const std::vector<cv::Point>& contour = *std::max_element(
            contours.begin(), contours.end(),
            [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                return a.size() < b.size();
            });

        PuzzlePiece piece;
        piece.center = QPointF(centers.at<double>(component, 0),
                               centers.at<double>(component, 1));
        for (const cv::Point& point : contour)
            piece.contour.append(QPointF(point.x + x, point.y + y));
        pieces.append(piece);
    }

    // Stable reading order, grouping nearby rows to avoid small offsets.
    std::sort(pieces.begin(), pieces.end(), [scale](const PuzzlePiece& a, const PuzzlePiece& b) {
        const int rowA = static_cast<int>(a.center.y() / (100.0 * scale));
        const int rowB = static_cast<int>(b.center.y() / (100.0 * scale));
        return rowA == rowB ? a.center.x() < b.center.x() : rowA < rowB;
    });
    for (int index = 0; index < pieces.size(); ++index)
        pieces[index].id = index + 1;

    return pieces;
}

void PuzzleDetector::detectTopContours(
    const QImage& image,
    QVector<PuzzlePiece>& pieces) const
{
    if (image.isNull() || pieces.isEmpty())
        return;

    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);

    const cv::Mat rgb(
        rgbImage.height(),
        rgbImage.width(),
        CV_8UC3,
        const_cast<uchar*>(rgbImage.constBits()),
        static_cast<size_t>(rgbImage.bytesPerLine())
        );

    cv::Mat lab;
    cv::cvtColor(rgb, lab, cv::COLOR_RGB2Lab);

    // Kleine Bilddetails und Bildrauschen abschwächen.
    cv::GaussianBlur(lab, lab, cv::Size(5, 5), 0.0);

    const double imageScale =
        static_cast<double>(image.width()) / 1536.0;

    const double maximumSearchDistance = 12.0 * imageScale;
    const double sampleDistance = 2.0 * imageScale;
    const double searchStep = std::max(0.5, imageScale);

    // OpenCV-Lab verwendet hier 8-Bit-Kanäle.
    // Dieser Wert ist ein experimenteller Schwellwert.
    const double minimumColorDifference = 12.0;

    const int smoothingRadius = 4;

    const auto sampleColor =
        [&lab](const QPointF& position, cv::Vec3b& color) -> bool
    {
        const int x = static_cast<int>(std::lround(position.x()));
        const int y = static_cast<int>(std::lround(position.y()));

        if (x < 0 || y < 0 || x >= lab.cols || y >= lab.rows)
            return false;

        color = lab.at<cv::Vec3b>(y, x);
        return true;
    };

    const auto colorDifference =
        [](const cv::Vec3b& first, const cv::Vec3b& second) -> double
    {
        const double dl =
            static_cast<double>(first[0]) - second[0];

        const double da =
            static_cast<double>(first[1]) - second[1];

        const double db =
            static_cast<double>(first[2]) - second[2];

        return std::sqrt(dl * dl + da * da + db * db);
    };

    for (PuzzlePiece& piece : pieces)
    {
        piece.topContour.clear();
        piece.topContourValid.clear();

        const int count = piece.contour.size();

        if (count < 10)
            continue;

        // Geschlossene Kontur in eine Binärmaske umwandeln.
        // Damit können wir die tatsächliche Innenseite bestimmen.

        cv::Mat pieceMask(
            image.height(),
            image.width(),
            CV_8UC1,
            cv::Scalar(0)
            );

        std::vector<cv::Point> contourPoints;
        contourPoints.reserve(count);

        for (const QPointF& point : piece.contour)
        {
            contourPoints.emplace_back(
                static_cast<int>(std::lround(point.x())),
                static_cast<int>(std::lround(point.y()))
                );
        }

        std::vector<std::vector<cv::Point>> contours;
        contours.push_back(contourPoints);

        cv::drawContours(
            pieceMask,
            contours,
            0,
            cv::Scalar(255),
            cv::FILLED
            );

        // Prüfen, ob ein Punkt innerhalb der Kontur liegt.

        const auto isInside =
            [&pieceMask](const QPointF& position) -> bool
        {
            const int x = static_cast<int>(std::lround(position.x()));
            const int y = static_cast<int>(std::lround(position.y()));

            if (x < 0 || y < 0 ||
                x >= pieceMask.cols ||
                y >= pieceMask.rows)
                return false;

            return pieceMask.at<uchar>(y, x) != 0;
        };

        QVector<double> offsets(count, 0.0);
        QVector<bool> valid(count, false);
        QVector<QPointF> normals(count);

        // 1. Lokale Richtung der Kontur bestimmen.

        for (int index = 0; index < count; ++index)
        {
            const int previous = (index - 4 + count) % count;
            const int next = (index + 4) % count;

            const QPointF tangent =
                piece.contour[next] - piece.contour[previous];

            const double length = std::hypot(
                tangent.x(),
                tangent.y()
                );

            if (length < 1e-6)
                continue;

            QPointF normal(
                -tangent.y() / length,
                tangent.x() / length
                );

            const QPointF position = piece.contour[index];

            // Die beiden möglichen Normalenrichtungen prüfen.
            // Der Testabstand muss groß genug sein, um nicht
            // versehentlich auf der Konturlinie selbst zu landen.

            const double normalTestDistance =
                std::max(3.0, 3.0 * imageScale);

            const QPointF positiveTest =
                position + normal * normalTestDistance;

            const QPointF negativeTest =
                position - normal * normalTestDistance;

            const bool positiveInside = isInside(positiveTest);
            const bool negativeInside = isInside(negativeTest);

            // Nur eine eindeutig erkannte Innenseite akzeptieren.

            if (positiveInside == negativeInside)
                continue;

            if (!positiveInside)
                normal = -normal;

            normals[index] = normal;

            // 2. Entlang der inneren Normalen nach einem
            // deutlichen Farbwechsel suchen.

            double bestDifference = 0.0;
            double bestOffset = 0.0;

            for (double offset = searchStep;
                 offset <= maximumSearchDistance;
                 offset += searchStep)
            {
                const QPointF outerPosition =
                    position + normal * (offset - sampleDistance);

                const QPointF innerPosition =
                    position + normal * (offset + sampleDistance);

                // Der innere Messpunkt muss weiterhin innerhalb
                // des Puzzleteils liegen.

                if (!isInside(innerPosition))
                    continue;

                cv::Vec3b outerColor;
                cv::Vec3b innerColor;

                if (!sampleColor(outerPosition, outerColor) ||
                    !sampleColor(innerPosition, innerColor))
                    continue;

                const double difference =
                    colorDifference(outerColor, innerColor);

                if (difference > bestDifference)
                {
                    bestDifference = difference;
                    bestOffset = offset;
                }
            }

            if (bestDifference >= minimumColorDifference)
            {
                offsets[index] = bestOffset;
                valid[index] = true;
            }
        }

        // 3. Lokale Ausreißer durch einen Medianfilter
        // reduzieren, ohne große Sprünge zu mitteln.

        QVector<double> smoothedOffsets(count, 0.0);

        for (int index = 0; index < count; ++index)
        {
            if (!valid[index])
                continue;

            QVector<double> neighbors;

            for (int relative = -smoothingRadius;
                 relative <= smoothingRadius;
                 ++relative)
            {
                const int neighbor =
                    (index + relative + count) % count;

                if (valid[neighbor])
                    neighbors.append(offsets[neighbor]);
            }

            if (neighbors.size() < 3)
            {
                valid[index] = false;
                continue;
            }

            std::sort(neighbors.begin(), neighbors.end());

            smoothedOffsets[index] =
                neighbors[neighbors.size() / 2];
        }

        // 4. Neue Kontur erzeugen.

        piece.topContour.reserve(count);
        piece.topContourValid.reserve(count);

        for (int index = 0; index < count; ++index)
        {
            const QPointF original = piece.contour[index];

            const QPointF refined =
                valid[index]
                    ? original + normals[index] * smoothedOffsets[index]
                    : original;

            piece.topContour.append(refined);
            piece.topContourValid.append(valid[index]);
        }

        // ------------------------------------------------------------
        // 5. Ausreißer relativ zur ursprünglichen Kontur korrigieren
        // ------------------------------------------------------------

        const int filterRadius = 10;
        const double maximumDeviation = 3.0 * imageScale;
        const int minimumNeighbors = 5;

        QVector<double> referenceOffsets(count, 0.0);
        QVector<bool> outliers(count, false);

        // Zunächst die bereits ermittelten Abstände untersuchen.
        // Die ursprünglichen Messwerte bleiben dabei unverändert.

        for (int index = 0; index < count; ++index)
        {
            if (!valid[index])
                continue;

            QVector<double> neighbors;

            for (int relative = -filterRadius;
                 relative <= filterRadius;
                 ++relative)
            {
                const int neighbor =
                    (index + relative + count) % count;

                if (valid[neighbor])
                    neighbors.append(smoothedOffsets[neighbor]);
            }

            if (neighbors.size() < minimumNeighbors)
                continue;

            std::sort(neighbors.begin(), neighbors.end());

            const double median =
                neighbors[neighbors.size() / 2];

            referenceOffsets[index] = median;

            if (std::abs(smoothedOffsets[index] - median) >
                maximumDeviation)
            {
                outliers[index] = true;
            }
        }

        // Ausreißer auf den lokalen Referenzabstand zurückführen.
        // Wir verwenden weiterhin die ursprüngliche Normale,
        // damit die Form der orangefarbenen Kontur erhalten bleibt.

        for (int index = 0; index < count; ++index)
        {
            if (!outliers[index])
                continue;

            const QPointF original = piece.contour[index];

            piece.topContour[index] =
                original + normals[index] * referenceOffsets[index];
        }

        // ------------------------------------------------------------
        // 6. Konservative Begrenzung und Stabilitätsprüfung
        // ------------------------------------------------------------

        const double maximumTopOffset = 6.0 * imageScale;
        const double maximumOffsetJump = 2.5 * imageScale;
        const int instabilityRadius = 3;
        const int minimumUnstableNeighbors = 2;

        QVector<double> finalOffsets(count, 0.0);
        QVector<bool> unstable(count, false);

        // Zuerst die bereits korrigierten Abstände bestimmen.

        for (int index = 0; index < count; ++index)
        {
            if (!piece.topContourValid[index])
                continue;

            const QPointF difference =
                piece.topContour[index] - piece.contour[index];

            const double offset =
                difference.x() * normals[index].x() +
                difference.y() * normals[index].y();

            finalOffsets[index] = std::max(0.0, offset);
        }

        // Starke Abstandssprünge erkennen.

        for (int index = 0; index < count; ++index)
        {
            if (!piece.topContourValid[index])
                continue;

            const int previous = (index - 1 + count) % count;
            const int next = (index + 1) % count;

            if (!piece.topContourValid[previous] ||
                !piece.topContourValid[next])
                continue;

            const double previousJump =
                std::abs(finalOffsets[index] - finalOffsets[previous]);

            const double nextJump =
                std::abs(finalOffsets[index] - finalOffsets[next]);

            if (previousJump > maximumOffsetJump ||
                nextJump > maximumOffsetJump)
            {
                unstable[index] = true;
            }
        }

        // Nur zusammenhängend auffällige Bereiche zurücksetzen.
        // Einzelne Sprünge werden zunächst nicht pauschal verworfen.

        QVector<bool> rejected(count, false);

        for (int index = 0; index < count; ++index)
        {
            if (!piece.topContourValid[index])
                continue;

            int unstableNeighbors = 0;

            for (int relative = -instabilityRadius;
                 relative <= instabilityRadius;
                 ++relative)
            {
                const int neighbor =
                    (index + relative + count) % count;

                if (unstable[neighbor])
                    ++unstableNeighbors;
            }

            if (unstableNeighbors >= minimumUnstableNeighbors)
                rejected[index] = true;
        }

        // Abschließend die grüne Kontur korrigieren.

        for (int index = 0; index < count; ++index)
        {
            if (!piece.topContourValid[index])
                continue;

            const QPointF original = piece.contour[index];

            if (rejected[index])
            {
                // Unzuverlässige Messung:
                // auf die ursprüngliche Kontur zurückfallen.
                piece.topContour[index] = original;
                continue;
            }

            // Zu große Verschiebungen begrenzen.
            const double correctedOffset =
                std::min(finalOffsets[index], maximumTopOffset);

            piece.topContour[index] =
                original + normals[index] * correctedOffset;
        }

    }
}
