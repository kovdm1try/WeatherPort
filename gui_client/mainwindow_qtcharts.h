#ifndef MAINWINDOW_QTCHARTS_H
#define MAINWINDOW_QTCHARTS_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTabWidget>
#include <QStatusBar>
#include <QGroupBox>
#include <QLineEdit>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>

#include "apiclient.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCurrentTemperatureReceived(double temperature, QDateTime timestamp);
    void onMeasurementsReceived(const QVector<DataPoint> &data);
    void onHourlyStatsReceived(const QVector<DataPoint> &data);
    void onDailyStatsReceived(const QVector<DataPoint> &data);
    void onStatisticsReceived(const Statistics &stats);
    void onError(const QString &error);

    void onRefreshClicked();
    void onPeriodChanged(int index);
    void onAutoRefreshToggled(bool enabled);
    void onServerUrlChanged();
    void onTabChanged(int index);

private:
    void setupUi();
    void setupConnections();
    void refreshData();
    void updateStatusBar(const QString &message);

    QChartView* createChartView(const QString &title, const QColor &color);
    void updateChart(QChartView *chartView, const QVector<DataPoint> &data,
                     const QString &dateFormat);

    ApiClient *m_apiClient;
    QTimer *m_refreshTimer;

    QLabel *m_currentTempLabel;
    QLabel *m_currentTimeLabel;

    QLabel *m_minTempLabel;
    QLabel *m_maxTempLabel;
    QLabel *m_avgTempLabel;
    QLabel *m_countLabel;

    QTabWidget *m_tabWidget;
    QChartView *m_measurementsChartView;
    QChartView *m_hourlyChartView;
    QChartView *m_dailyChartView;

    QComboBox *m_periodCombo;
    QPushButton *m_refreshButton;
    QPushButton *m_autoRefreshButton;
    QLineEdit *m_serverUrlEdit;
    QSpinBox *m_refreshIntervalSpin;

    bool m_autoRefresh;
    int m_currentMeasurementHours;
    int m_currentHourlyDays;
    int m_currentDailyDays;
};

#endif // MAINWINDOW_QTCHARTS_H
