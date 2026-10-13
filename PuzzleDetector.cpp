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
