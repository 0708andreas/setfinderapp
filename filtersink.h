#ifndef FILTERSINK_H
#define FILTERSINK_H

#include <QVideoSink>
#include <QVideoFrame>
#include <QObject>
#include <QVideoWidget>

class FilterSink : public QVideoSink
{
public:
    FilterSink(QObject* parent=nullptr);
    void setVideoFrame(const QVideoFrame &frame);
    void setThreshold(int);
    void setViewfinder(QVideoWidget*);

private:
    QVideoWidget* m_viewfinder;
    int m_threshold=100;
};

#endif // FILTERSINK_H
