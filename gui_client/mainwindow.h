#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

#include "apiclient.h"
#include "temperaturechart.h"

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

    ApiClient *m_apiClient;
    QTimer *m_refreshTimer;

    QLabel *m_currentTempLabel;
    QLabel *m_currentTimeLabel;

    QLabel *m_minTempLabel;
    QLabel *m_maxTempLabel;
    QLabel *m_avgTempLabel;
    QLabel *m_countLabel;

    QTabWidget *m_tabWidget;
    TemperatureChart *m_measurementsChart;
    TemperatureChart *m_hourlyChart;
    TemperatureChart *m_dailyChart;

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

#endif // MAINWINDOW_H
