#include "apiclient.h"
#include <QUrlQuery>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_serverUrl("http://localhost:9847")
{
}

void ApiClient::setServerUrl(const QString &url) {
    m_serverUrl = url;
}

QString ApiClient::serverUrl() const {
    return m_serverUrl;
}

void ApiClient::fetchCurrentTemperature() {
    QUrl url(m_serverUrl + "/api/current");
    QNetworkRequest request(url);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onCurrentReply(reply);
    });
}

void ApiClient::fetchMeasurements(int hours) {
    QUrl url(m_serverUrl + "/api/measurements");
    QUrlQuery query;
    query.addQueryItem("hours", QString::number(hours));
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onMeasurementsReply(reply);
    });
}

void ApiClient::fetchHourlyStats(int days) {
    QUrl url(m_serverUrl + "/api/hourly");
    QUrlQuery query;
    query.addQueryItem("days", QString::number(days));
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHourlyReply(reply);
    });
}

void ApiClient::fetchDailyStats(int days) {
    QUrl url(m_serverUrl + "/api/daily");
    QUrlQuery query;
    query.addQueryItem("days", QString::number(days));
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDailyReply(reply);
    });
}

void ApiClient::fetchStatistics() {
    QUrl url(m_serverUrl + "/api/stats");
    QNetworkRequest request(url);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onStatsReply(reply);
    });
}

void ApiClient::onCurrentReply(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(tr("Ошибка подключения: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit errorOccurred(tr("Неверный формат ответа"));
        return;
    }

    QJsonObject obj = doc.object();
    double temperature = obj["temperature"].toDouble();
    qint64 timestamp = obj["timestamp"].toVariant().toLongLong();
    QDateTime dt = QDateTime::fromSecsSinceEpoch(timestamp);

    emit currentTemperatureReceived(temperature, dt);
}

void ApiClient::onMeasurementsReply(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(tr("Ошибка получения измерений: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        emit errorOccurred(tr("Неверный формат данных измерений"));
        return;
    }

    QVector<DataPoint> data = parseDataArray(doc.array(), false);
    emit measurementsReceived(data);
}

void ApiClient::onHourlyReply(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(tr("Ошибка получения почасовой статистики: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        emit errorOccurred(tr("Неверный формат почасовой статистики"));
        return;
    }

    QVector<DataPoint> data = parseDataArray(doc.array(), true);
    emit hourlyStatsReceived(data);
}

void ApiClient::onDailyReply(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(tr("Ошибка получения дневной статистики: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        emit errorOccurred(tr("Неверный формат дневной статистики"));
        return;
    }

    QVector<DataPoint> data = parseDataArray(doc.array(), true);
    emit dailyStatsReceived(data);
}

void ApiClient::onStatsReply(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(tr("Ошибка получения статистики: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit errorOccurred(tr("Неверный формат статистики"));
        return;
    }

    QJsonObject obj = doc.object();
    Statistics stats;
    stats.min = obj["min"].toDouble();
    stats.max = obj["max"].toDouble();
    stats.avg = obj["avg"].toDouble();
    stats.count = obj["count"].toInt();

    emit statisticsReceived(stats);
}

QVector<DataPoint> ApiClient::parseDataArray(const QJsonArray &array, bool isAvgTemp) {
    QVector<DataPoint> result;
    result.reserve(array.size());

    for (const QJsonValue &val : array) {
        if (!val.isObject()) continue;

        QJsonObject obj = val.toObject();
        DataPoint point;

        qint64 timestamp = obj["timestamp"].toVariant().toLongLong();
        point.timestamp = QDateTime::fromSecsSinceEpoch(timestamp);

        if (isAvgTemp) {
            point.temperature = obj["avg_temp"].toDouble();
            point.count = obj["count"].toInt();
        } else {
            point.temperature = obj["temperature"].toDouble();
        }

        result.append(point);
    }

    return result;
}
