#include "filtersink.h"
#include <QVideoSurfaceFormat>
#include <opencv2/opencv.hpp>

FilterSink::FilterSink(QObject* parent) : QVideoSink(parent)
{

}
void FilterSink::setVideoFrame(const QVideoFrame &frame) {
    cv::Mat img = cv::Mat(frame.height(), frame.width(), CV_8UC4,
                          const_cast<uchar*>(frame.bits(0)),
                          frame.bytesPerLine(0)).clone();
    cv::cvtColor(img, img, cv::COLOR_RGBA2BGR);
    cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
    cv::Mat gray;
    cv::Mat th;
    cv::Mat img_dec;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, th, m_threshold, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);
    cv::cvtColor(th, img_dec, cv::COLOR_GRAY2RGB);
    QImage qimg_dec = QImage(img_dec.data, img_dec.cols, img_dec.rows,
                             img_dec.step, QImage::Format_RGB888);

}
void FilterSink::setThreshold(int threshold) {
    m_threshold = threshold;
}
void FilterSink::setViewfinder(QVideoWidget* viewfinder) {
    m_viewfinder = viewfinder;
}
