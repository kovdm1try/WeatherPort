#include "temperaturechart.h"
#include <qwt_plot_canvas.h>
#include <qwt_scale_widget.h>
#include <QFont>

TemperatureChart::TemperatureChart(QWidget *parent)
    : QwtPlot(parent)
    , m_curve(nullptr)
    , m_grid(nullptr)
    , m_dateScale(nullptr)
{
    setupChart();
}

void TemperatureChart::setupChart() {
    QwtPlotCanvas *canvas = new QwtPlotCanvas();
    canvas->setFrameStyle(QFrame::Box | QFrame::Plain);
    canvas->setLineWidth(1);
    canvas->setPalette(QPalette(Qt::white));
    setCanvas(canvas);

    m_grid = new QwtPlotGrid();
    m_grid->setPen(QPen(Qt::gray, 0, Qt::DotLine));
    m_grid->attach(this);

    setAxisTitle(QwtPlot::yLeft, tr("Температура, °C"));
    setAxisTitle(QwtPlot::xBottom, tr("Время"));

    m_dateScale = new DateScaleDraw("dd.MM HH:mm");
    setAxisScaleDraw(QwtPlot::xBottom, m_dateScale);

    m_curve = new QwtPlotCurve(tr("Температура"));
    m_curve->setPen(QPen(QColor(0, 120, 215), 2));
    m_curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);

    QwtSymbol *symbol = new QwtSymbol(
        QwtSymbol::Ellipse,
        QBrush(QColor(0, 120, 215)),
        QPen(QColor(0, 90, 180), 1),
        QSize(6, 6)
    );
    m_curve->setSymbol(symbol);

    m_curve->attach(this);

    QwtLegend *legend = new QwtLegend();
    legend->setDefaultItemMode(QwtLegendData::ReadOnly);
    insertLegend(legend, QwtPlot::BottomLegend);

    QFont font = QwtPlot::axisFont(QwtPlot::xBottom);
    font.setPointSize(9);
    setAxisFont(QwtPlot::xBottom, font);
    setAxisFont(QwtPlot::yLeft, font);

    setMinimumSize(400, 300);
}

void TemperatureChart::setData(const QVector<DataPoint> &data) {
    m_xData.clear();
    m_yData.clear();

    if (data.isEmpty()) {
        m_curve->setSamples(m_xData, m_yData);
        replot();
        return;
    }

    m_xData.reserve(data.size());
    m_yData.reserve(data.size());

    for (const DataPoint &point : data) {
        m_xData.append(point.timestamp.toMSecsSinceEpoch());
        m_yData.append(point.temperature);
    }

    m_curve->setSamples(m_xData, m_yData);

    setAxisAutoScale(QwtPlot::xBottom);
    setAxisAutoScale(QwtPlot::yLeft);

    replot();
}

void TemperatureChart::setTitle(const QString &title) {
    QwtPlot::setTitle(title);
    m_curve->setTitle(title);
}

void TemperatureChart::setDateFormat(const QString &format) {
    if (m_dateScale) {
        m_dateScale->setFormat(format);
        replot();
    }
}

void TemperatureChart::clearData() {
    m_xData.clear();
    m_yData.clear();
    m_curve->setSamples(m_xData, m_yData);
    replot();
}

void TemperatureChart::setCurveColor(const QColor &color) {
    m_curve->setPen(QPen(color, 2));

    QwtSymbol *symbol = new QwtSymbol(
        QwtSymbol::Ellipse,
        QBrush(color),
        QPen(color.darker(120), 1),
        QSize(6, 6)
    );
    m_curve->setSymbol(symbol);

    replot();
}
