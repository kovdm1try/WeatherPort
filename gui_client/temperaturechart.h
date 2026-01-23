#ifndef TEMPERATURECHART_H
#define TEMPERATURECHART_H

#include <QWidget>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_legend.h>
#include <qwt_scale_draw.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>
#include <qwt_text.h>

#include "apiclient.h"

class DateScaleDraw : public QwtScaleDraw {
public:
    DateScaleDraw(const QString &format = "dd.MM HH:mm")
        : m_format(format) {}

    virtual QwtText label(double value) const override {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(value));
        return dt.toString(m_format);
    }

    void setFormat(const QString &format) {
        m_format = format;
    }

private:
    QString m_format;
};

class TemperatureChart : public QwtPlot {
    Q_OBJECT

public:
    explicit TemperatureChart(QWidget *parent = nullptr);

    void setData(const QVector<DataPoint> &data);
    void setTitle(const QString &title);
    void setDateFormat(const QString &format);
    void clearData();
    void setCurveColor(const QColor &color);

private:
    void setupChart();

    QwtPlotCurve *m_curve;
    QwtPlotGrid *m_grid;
    DateScaleDraw *m_dateScale;

    QVector<double> m_xData;
    QVector<double> m_yData;
};

#endif // TEMPERATURECHART_H
