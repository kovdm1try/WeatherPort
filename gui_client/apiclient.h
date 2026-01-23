#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QVector>

struct DataPoint {
    QDateTime timestamp;
    double temperature;
    int count = 1;
};

struct Statistics {
    double min;
    double max;
    double avg;
    int count;
};

class ApiClient : public QObject {
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    void setServerUrl(const QString &url);
    QString serverUrl() const;

    void fetchCurrentTemperature();
    void fetchMeasurements(int hours = 24);
    void fetchHourlyStats(int days = 7);
    void fetchDailyStats(int days = 30);
    void fetchStatistics();

signals:
    void currentTemperatureReceived(double temperature, QDateTime timestamp);
    void measurementsReceived(const QVector<DataPoint> &data);
    void hourlyStatsReceived(const QVector<DataPoint> &data);
    void dailyStatsReceived(const QVector<DataPoint> &data);
    void statisticsReceived(const Statistics &stats);
    void errorOccurred(const QString &error);

private slots:
    void onCurrentReply(QNetworkReply *reply);
    void onMeasurementsReply(QNetworkReply *reply);
    void onHourlyReply(QNetworkReply *reply);
    void onDailyReply(QNetworkReply *reply);
    void onStatsReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_manager;
    QString m_serverUrl;

    QVector<DataPoint> parseDataArray(const QJsonArray &array, bool isAvgTemp = false);
};

#endif // APICLIENT_H
