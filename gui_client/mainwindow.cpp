#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFont>
#include <QFrame>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_apiClient(new ApiClient(this))
    , m_refreshTimer(new QTimer(this))
    , m_autoRefresh(false)
    , m_currentMeasurementHours(24)
    , m_currentHourlyDays(7)
    , m_currentDailyDays(30)
{
    setupUi();
    setupConnections();
    refreshData();
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUi() {
    setWindowTitle(tr("WeatherPort - Мониторинг температуры"));
    setMinimumSize(900, 700);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QGroupBox *settingsGroup = new QGroupBox(tr("Настройки"));
    QHBoxLayout *settingsLayout = new QHBoxLayout(settingsGroup);

    settingsLayout->addWidget(new QLabel(tr("Сервер:")));
    m_serverUrlEdit = new QLineEdit("http://localhost:9847");
    m_serverUrlEdit->setFixedWidth(200);
    settingsLayout->addWidget(m_serverUrlEdit);

    settingsLayout->addWidget(new QLabel(tr("Период:")));
    m_periodCombo = new QComboBox();
    m_periodCombo->addItem(tr("1 час"), 1);
    m_periodCombo->addItem(tr("6 часов"), 6);
    m_periodCombo->addItem(tr("12 часов"), 12);
    m_periodCombo->addItem(tr("24 часа"), 24);
    m_periodCombo->setCurrentIndex(3);
    settingsLayout->addWidget(m_periodCombo);

    settingsLayout->addWidget(new QLabel(tr("Интервал (сек):")));
    m_refreshIntervalSpin = new QSpinBox();
    m_refreshIntervalSpin->setRange(1, 60);
    m_refreshIntervalSpin->setValue(5);
    settingsLayout->addWidget(m_refreshIntervalSpin);

    m_autoRefreshButton = new QPushButton(tr("Автообновление"));
    m_autoRefreshButton->setCheckable(true);
    settingsLayout->addWidget(m_autoRefreshButton);

    m_refreshButton = new QPushButton(tr("Обновить"));
    settingsLayout->addWidget(m_refreshButton);

    settingsLayout->addStretch();

    mainLayout->addWidget(settingsGroup);

    QHBoxLayout *infoLayout = new QHBoxLayout();

    QGroupBox *currentGroup = new QGroupBox(tr("Текущая температура"));
    QVBoxLayout *currentLayout = new QVBoxLayout(currentGroup);

    m_currentTempLabel = new QLabel("--.- °C");
    QFont tempFont = m_currentTempLabel->font();
    tempFont.setPointSize(48);
    tempFont.setBold(true);
    m_currentTempLabel->setFont(tempFont);
    m_currentTempLabel->setAlignment(Qt::AlignCenter);
    m_currentTempLabel->setStyleSheet("color: #0078D7;");
    currentLayout->addWidget(m_currentTempLabel);

    m_currentTimeLabel = new QLabel(tr("Время: --"));
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);
    currentLayout->addWidget(m_currentTimeLabel);

    infoLayout->addWidget(currentGroup);

    QGroupBox *statsGroup = new QGroupBox(tr("Статистика за весь период"));
    QGridLayout *statsLayout = new QGridLayout(statsGroup);

    statsLayout->addWidget(new QLabel(tr("Минимум:")), 0, 0);
    m_minTempLabel = new QLabel("--.- °C");
    m_minTempLabel->setStyleSheet("font-weight: bold; color: #0078D7;");
    statsLayout->addWidget(m_minTempLabel, 0, 1);

    statsLayout->addWidget(new QLabel(tr("Максимум:")), 1, 0);
    m_maxTempLabel = new QLabel("--.- °C");
    m_maxTempLabel->setStyleSheet("font-weight: bold; color: #E81123;");
    statsLayout->addWidget(m_maxTempLabel, 1, 1);

    statsLayout->addWidget(new QLabel(tr("Среднее:")), 2, 0);
    m_avgTempLabel = new QLabel("--.- °C");
    m_avgTempLabel->setStyleSheet("font-weight: bold; color: #107C10;");
    statsLayout->addWidget(m_avgTempLabel, 2, 1);

    statsLayout->addWidget(new QLabel(tr("Измерений:")), 3, 0);
    m_countLabel = new QLabel("--");
    m_countLabel->setStyleSheet("font-weight: bold;");
    statsLayout->addWidget(m_countLabel, 3, 1);

    infoLayout->addWidget(statsGroup);

    mainLayout->addLayout(infoLayout);

    m_tabWidget = new QTabWidget();

    m_measurementsChart = new TemperatureChart();
    m_measurementsChart->setTitle(tr("Детальные измерения"));
    m_measurementsChart->setCurveColor(QColor(0, 120, 215));
    m_tabWidget->addTab(m_measurementsChart, tr("Измерения"));

    m_hourlyChart = new TemperatureChart();
    m_hourlyChart->setTitle(tr("Средняя температура по часам"));
    m_hourlyChart->setDateFormat("dd.MM HH:00");
    m_hourlyChart->setCurveColor(QColor(136, 23, 152));
    m_tabWidget->addTab(m_hourlyChart, tr("По часам"));

    m_dailyChart = new TemperatureChart();
    m_dailyChart->setTitle(tr("Средняя температура по дням"));
    m_dailyChart->setDateFormat("dd.MM.yyyy");
    m_dailyChart->setCurveColor(QColor(16, 124, 16));
    m_tabWidget->addTab(m_dailyChart, tr("По дням"));

    mainLayout->addWidget(m_tabWidget, 1);

    statusBar()->showMessage(tr("Готов"));
}

void MainWindow::setupConnections() {
    connect(m_apiClient, &ApiClient::currentTemperatureReceived,
            this, &MainWindow::onCurrentTemperatureReceived);
    connect(m_apiClient, &ApiClient::measurementsReceived,
            this, &MainWindow::onMeasurementsReceived);
    connect(m_apiClient, &ApiClient::hourlyStatsReceived,
            this, &MainWindow::onHourlyStatsReceived);
    connect(m_apiClient, &ApiClient::dailyStatsReceived,
            this, &MainWindow::onDailyStatsReceived);
    connect(m_apiClient, &ApiClient::statisticsReceived,
            this, &MainWindow::onStatisticsReceived);
    connect(m_apiClient, &ApiClient::errorOccurred,
            this, &MainWindow::onError);

    connect(m_refreshButton, &QPushButton::clicked,
            this, &MainWindow::onRefreshClicked);
    connect(m_periodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPeriodChanged);
    connect(m_autoRefreshButton, &QPushButton::toggled,
            this, &MainWindow::onAutoRefreshToggled);
    connect(m_serverUrlEdit, &QLineEdit::editingFinished,
            this, &MainWindow::onServerUrlChanged);
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::onTabChanged);

    connect(m_refreshTimer, &QTimer::timeout,
            this, &MainWindow::refreshData);
}

void MainWindow::refreshData() {
    updateStatusBar(tr("Загрузка данных..."));

    m_apiClient->fetchCurrentTemperature();
    m_apiClient->fetchStatistics();

    int tabIndex = m_tabWidget->currentIndex();
    switch (tabIndex) {
    case 0:
        m_apiClient->fetchMeasurements(m_currentMeasurementHours);
        break;
    case 1:
        m_apiClient->fetchHourlyStats(m_currentHourlyDays);
        break;
    case 2:
        m_apiClient->fetchDailyStats(m_currentDailyDays);
        break;
    }
}

void MainWindow::onCurrentTemperatureReceived(double temperature, QDateTime timestamp) {
    m_currentTempLabel->setText(QString("%1 °C").arg(temperature, 0, 'f', 2));
    m_currentTimeLabel->setText(tr("Время: %1").arg(timestamp.toString("dd.MM.yyyy HH:mm:ss")));
    updateStatusBar(tr("Данные обновлены"));
}

void MainWindow::onMeasurementsReceived(const QVector<DataPoint> &data) {
    m_measurementsChart->setData(data);
    updateStatusBar(tr("Загружено %1 измерений").arg(data.size()));
}

void MainWindow::onHourlyStatsReceived(const QVector<DataPoint> &data) {
    m_hourlyChart->setData(data);
    updateStatusBar(tr("Загружено %1 часовых записей").arg(data.size()));
}

void MainWindow::onDailyStatsReceived(const QVector<DataPoint> &data) {
    m_dailyChart->setData(data);
    updateStatusBar(tr("Загружено %1 дневных записей").arg(data.size()));
}

void MainWindow::onStatisticsReceived(const Statistics &stats) {
    m_minTempLabel->setText(QString("%1 °C").arg(stats.min, 0, 'f', 2));
    m_maxTempLabel->setText(QString("%1 °C").arg(stats.max, 0, 'f', 2));
    m_avgTempLabel->setText(QString("%1 °C").arg(stats.avg, 0, 'f', 2));
    m_countLabel->setText(QString::number(stats.count));
}

void MainWindow::onError(const QString &error) {
    updateStatusBar(tr("Ошибка: %1").arg(error));
    QMessageBox::warning(this, tr("Ошибка"), error);
}

void MainWindow::onRefreshClicked() {
    refreshData();
}

void MainWindow::onPeriodChanged(int index) {
    m_currentMeasurementHours = m_periodCombo->itemData(index).toInt();

    switch (index) {
    case 0:
        m_currentHourlyDays = 1;
        m_currentDailyDays = 7;
        break;
    case 1:
        m_currentHourlyDays = 3;
        m_currentDailyDays = 14;
        break;
    case 2:
        m_currentHourlyDays = 7;
        m_currentDailyDays = 21;
        break;
    case 3:
    default:
        m_currentHourlyDays = 7;
        m_currentDailyDays = 30;
        break;
    }

    refreshData();
}

void MainWindow::onAutoRefreshToggled(bool enabled) {
    m_autoRefresh = enabled;
    if (enabled) {
        int interval = m_refreshIntervalSpin->value() * 1000;
        m_refreshTimer->start(interval);
        m_autoRefreshButton->setText(tr("Стоп"));
        updateStatusBar(tr("Автообновление включено (каждые %1 сек)").arg(m_refreshIntervalSpin->value()));
    } else {
        m_refreshTimer->stop();
        m_autoRefreshButton->setText(tr("Автообновление"));
        updateStatusBar(tr("Автообновление выключено"));
    }
}

void MainWindow::onServerUrlChanged() {
    QString url = m_serverUrlEdit->text().trimmed();
    if (!url.isEmpty()) {
        m_apiClient->setServerUrl(url);
        updateStatusBar(tr("Сервер изменен: %1").arg(url));
        refreshData();
    }
}

void MainWindow::onTabChanged(int index) {
    switch (index) {
    case 0:
        m_apiClient->fetchMeasurements(m_currentMeasurementHours);
        break;
    case 1:
        m_apiClient->fetchHourlyStats(m_currentHourlyDays);
        break;
    case 2:
        m_apiClient->fetchDailyStats(m_currentDailyDays);
        break;
    }
}

void MainWindow::updateStatusBar(const QString &message) {
    statusBar()->showMessage(message, 5000);
}
