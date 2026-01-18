const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
        legend: {
            labels: { color: '#333' }
        }
    },
    scales: {
        x: {
            type: 'time',
            time: {
                displayFormats: {
                    hour: 'HH:mm',
                    day: 'dd.MM'
                }
            },
            ticks: { color: '#666' },
            grid: { color: '#eee' }
        },
        y: {
            ticks: {
                color: '#666',
                callback: function(value) {
                    return value + '°C';
                }
            },
            grid: { color: '#eee' }
        }
    }
};

let measurementsChart, hourlyChart, dailyChart;

function formatDate(timestamp) {
    return new Date(timestamp * 1000).toLocaleString('ru-RU');
}

async function loadCurrent() {
    try {
        const resp = await fetch('/api/current');
        const data = await resp.json();

        if (data.error) {
            document.getElementById('currentTemp').textContent = 'Ошибка';
            document.getElementById('lastUpdate').textContent = data.error;
            return;
        }

        document.getElementById('currentTemp').textContent =
            data.temperature.toFixed(1) + '°C';
        document.getElementById('lastUpdate').textContent =
            data.timestamp > 0 ? 'Обновлено: ' + data.timestamp_formatted : 'Нет данных';
    } catch (e) {
        document.getElementById('currentTemp').textContent = 'Ошибка';
        document.getElementById('lastUpdate').textContent = 'Сервер недоступен';
    }
}

async function loadStats() {
    try {
        const resp = await fetch('/api/stats');
        const data = await resp.json();

        if (data.error) return;

        document.getElementById('minTemp').textContent =
            data.count > 0 ? data.min.toFixed(1) + '°C' : '--';
        document.getElementById('maxTemp').textContent =
            data.count > 0 ? data.max.toFixed(1) + '°C' : '--';
        document.getElementById('avgTemp').textContent =
            data.count > 0 ? data.avg.toFixed(1) + '°C' : '--';
        document.getElementById('countMeasurements').textContent = data.count;
    } catch (e) {
        console.error('Stats error:', e);
    }
}

async function loadMeasurements(hours = 24) {
    try {
        const resp = await fetch(`/api/measurements?hours=${hours}`);
        const data = await resp.json();

        if (data.error) return;

        const labels = data.map(d => new Date(d.timestamp * 1000));
        const values = data.map(d => d.temperature);

        if (measurementsChart) {
            measurementsChart.destroy();
        }

        const ctx = document.getElementById('measurementsChart').getContext('2d');
        measurementsChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Температура',
                    data: values,
                    borderColor: '#2563eb',
                    backgroundColor: 'rgba(37, 99, 235, 0.1)',
                    fill: true,
                    tension: 0.3
                }]
            },
            options: chartOptions
        });

        const tableBody = document.getElementById('measurementsTable');
        if (data.length === 0) {
            tableBody.innerHTML = '<tr><td colspan="2">Нет данных</td></tr>';
            return;
        }

        const recent = data.slice(-20).reverse();
        tableBody.innerHTML = recent.map(d => `
            <tr>
                <td>${formatDate(d.timestamp)}</td>
                <td>${d.temperature.toFixed(1)}°C</td>
            </tr>
        `).join('');

        updateActiveTab(event, hours);
    } catch (e) {
        console.error('Measurements error:', e);
    }
}

async function loadHourly(days = 7) {
    try {
        const resp = await fetch(`/api/hourly?days=${days}`);
        const data = await resp.json();

        if (data.error || !Array.isArray(data)) return;

        const labels = data.map(d => new Date(d.timestamp * 1000));
        const values = data.map(d => d.avg_temp);

        if (hourlyChart) {
            hourlyChart.destroy();
        }

        const ctx = document.getElementById('hourlyChart').getContext('2d');
        hourlyChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Средняя температура',
                    data: values,
                    borderColor: '#8b5cf6',
                    backgroundColor: 'rgba(139, 92, 246, 0.1)',
                    fill: true,
                    tension: 0.3
                }]
            },
            options: chartOptions
        });

        updateActiveTab(event, days);
    } catch (e) {
        console.error('Hourly error:', e);
    }
}

async function loadDaily(days = 30) {
    try {
        const resp = await fetch(`/api/daily?days=${days}`);
        const data = await resp.json();

        if (data.error || !Array.isArray(data)) return;

        const labels = data.map(d => new Date(d.timestamp * 1000));
        const values = data.map(d => d.avg_temp);

        if (dailyChart) {
            dailyChart.destroy();
        }

        const ctx = document.getElementById('dailyChart').getContext('2d');
        dailyChart = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Средняя температура',
                    data: values,
                    backgroundColor: 'rgba(16, 185, 129, 0.7)',
                    borderColor: '#10b981',
                    borderWidth: 1
                }]
            },
            options: {
                ...chartOptions,
                scales: {
                    ...chartOptions.scales,
                    x: {
                        ...chartOptions.scales.x,
                        time: {
                            displayFormats: { day: 'dd.MM' }
                        }
                    }
                }
            }
        });

        updateActiveTab(event, days);
    } catch (e) {
        console.error('Daily error:', e);
    }
}

function updateActiveTab(event, value) {
    if (!event || !event.target) return;
    const parent = event.target.parentElement;
    parent.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
    event.target.classList.add('active');
}

function startAutoRefresh() {
    loadCurrent();
    loadStats();

    setInterval(() => {
        loadCurrent();
        loadStats();
    }, 5000);
}

document.addEventListener('DOMContentLoaded', () => {
    startAutoRefresh();
    loadMeasurements(24);
    loadHourly(7);
    loadDaily(30);
});
