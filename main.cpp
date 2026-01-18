#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <csignal>
#include <clocale>
#include <cstdlib>

#include "libs/SerialPort.h"

#ifdef _WIN32
    #include <windows.h>
#endif

using namespace std;

// Пути до фалов логов
const string TEMP_LOG = "log/tempreture.log";
const string TEMP_HOURLY_LOG = "log/tempreture_mean_hour.log";
const string TEMP_DAILY_LOG = "log/tempreture_mean_day.log";

// Структура 1 записи в логах(время и температура)
struct TimeTemp {
    chrono::system_clock::time_point time;
    double temp{};
};

// Преобразование time_point в строку вида DD/MM/YYYY HH:MM
string TimePoint2Sting(const chrono::system_clock::time_point tp) {
    time_t t = chrono::system_clock::to_time_t(tp);

    // Structure holding a calendar date and time broken down into its components(https://en.cppreference.com/w/c/chrono/tm)
    tm tm{};

#ifdef _WIN32
        localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    // Собираем строку DD:MM:YYYY HH:MM в поток ss с помощью метода put_time
    ostringstream oss;
    oss << put_time(&tm, "%Y/%m/%d %H:%M:%S");

    return oss.str();
}

/*
params:
    date: дата в формате "%Y/%m/%d %H:%M:%S"
    tp: ссылка на переменну/ куда положим распаршенное значение time_point
 return:
    bool: True если получилось распарсить, False - если не получилось
 */
bool String2TimePoint(const string &date, chrono::system_clock::time_point &tp) {
    tm tm{};
    istringstream iss(date);

    iss >> get_time(&tm, "%Y/%m/%d %H:%M:%S");
    if (iss.fail()) {
        return false;
    }

    time_t t = mktime(&tm);
    if (t < 0) {
        return false;
    }
    tp = chrono::system_clock::from_time_t(t);
    return true;
}


/*
params:
    line: строка лога %Y/%m/%d %H:%M:%S|temp
    tt: ссылка на переменную со структурой TimeTemp куда положим распаршенное значение
return:
    bool: True если получилось распарсить, False - если не получилось
 */
bool parseLogLine(const string &line, TimeTemp &tt) {
    const auto sep_pos = line.find('|');

    // нет разделителя
    if (sep_pos == string::npos) {
        return false;
    }

    const string time_part = line.substr(0, sep_pos);
    const string temp_part = line.substr(sep_pos + 1);

    // Парсим время
    chrono::system_clock::time_point tp;
    if (!String2TimePoint(time_part, tp)) {
        return false;
    }

    // Парсим температуру как double(stod -> return double)
    try {
        const double temp = stod(temp_part);
        tt.time = tp;
        tt.temp = temp;
        return true;
    } catch (...) {
        return false;
    }
}

void appendRotate(const string& file_path,
                  const TimeTemp& current,
                  const chrono::hours& max_age)
{
/*
appendRotate
  Добавление записи в логи через промежуток времени.
params:
  file_path — путь к лог-файлу
  current   — новая запись
  max_age   — максимальный возраст записей (в часах),
              всё что старше — выкидывается
return:
    void
*/
    vector<TimeTemp> keep; // то что оставляем
    const auto now = chrono::system_clock::now();
    const auto min_time = now - max_age;

    // Читаем старый лог, оставляем только то что попадает в диапозон
    ifstream fin(file_path);
    if (fin) {
        string line;
        while (getline(fin, line)) {
            TimeTemp tt;
            if (!parseLogLine(line, tt)) continue;
            if (tt.time >= min_time) {
                keep.push_back(tt);
            }
        }
    }

    keep.push_back(current);

    // перезаписываем файл
    ofstream fout(file_path, ios::trunc);
    for (const auto& tt : keep) {
        fout << TimePoint2Sting(tt.time) << '|' << tt.temp << '\n';
    }
}

// Средние дневные логи, хранятся год
void appendKeepCurrentYear(const string& file_path, const TimeTemp& current) {
    vector<TimeTemp> keep;

    // Текущий год
    const auto now = chrono::system_clock::now();
    time_t t_now = chrono::system_clock::to_time_t(now);
    tm tm_now{};
    #ifdef _WIN32
        localtime_s(&tm_now, &t_now);
    #else
        localtime_r(&t_now, &tm_now);
    #endif
    const int cur_year = tm_now.tm_year + 1900;

    // Читаем старые записи и оставляем только за текщий год
    ifstream fin(file_path);
    if (fin) {
        string line;
        while (getline(fin, line)) {
            TimeTemp tt;
            if (!parseLogLine(line, tt)) continue;

            time_t t = chrono::system_clock::to_time_t(tt.time);
            tm tm_entry{};
    #ifdef _WIN32
                localtime_s(&tm_entry, &t);
    #else
                localtime_r(&t, &tm_entry);
    #endif
            const int y = tm_entry.tm_year + 1900;
            if (y == cur_year) {
                keep.push_back(tt);
            }
        }
    }

    keep.push_back(current);

    // Перезаписываем файл
    ofstream fout(file_path, ios::trunc);
    for (const auto& tt : keep) {
        fout << TimePoint2Sting(tt.time) << '|' << tt.temp << '\n';
    }
}

// Агрегатор средних значений
struct MeanAgg {
    long long hour_key = -1; // к какому часу принадлежат измерения
    double hour_sum = 0.0; // сумма температуры
    long long hour_cnt = 0; // количество измерений
    chrono::system_clock::time_point hour_start_time; // время начала текущего часа

    long long day_key  = -1; // к какому дню принадлежат измерения
    double day_sum  = 0.0; // сумма температуры
    long long day_cnt  = 0; // количество измерений
    chrono::system_clock::time_point day_start_time; // время начала текущего дня

    // Считаем ключ дня: YYYYMMDD
    static long long dayKey(const tm& tm_time) {
        return (tm_time.tm_year + 1900) * 10000LL
             + (tm_time.tm_mon + 1)   * 100LL
             + tm_time.tm_mday;
    }

    // Считаем ключ часа: YYYYMMDDHH
    static long long hourKey(const tm& tm_time) {
        return dayKey(tm_time) * 100LL + tm_time.tm_hour;
    }

    // Сброс и запись накопленных данных (вызывается при завершении программы)
    void flush() {
        if (hour_cnt > 0) {
            TimeTemp hour_mean;
            hour_mean.time = hour_start_time;
            hour_mean.temp = hour_sum / hour_cnt;
            appendRotate(TEMP_HOURLY_LOG, hour_mean, chrono::hours(24 * 30));
        }
        if (day_cnt > 0) {
            TimeTemp day_mean;
            day_mean.time = day_start_time;
            day_mean.temp = day_sum / day_cnt;
            appendKeepCurrentYear(TEMP_DAILY_LOG, day_mean);
        }
    }

    // Добавляем одно измерение и при необходимости
    // пишем средние значения за завершившийся час / день
    void add(const TimeTemp& tt) {
        using namespace chrono;

        // Переводим время измерения в локальное tm
        time_t t = system_clock::to_time_t(tt.time);
        tm tm_entry{};
        #ifdef _WIN32
            localtime_s(&tm_entry, &t);
        #else
            localtime_r(&t, &tm_entry);
        #endif

        const long long cur_day  = dayKey(tm_entry);
        const long long cur_hour = hourKey(tm_entry);

        // первое измерение часа
        if (hour_key == -1) {
            hour_key = cur_hour;
            hour_start_time = tt.time;
        } else if (cur_hour != hour_key && hour_cnt > 0) {
            // при смене часа считаем среднее за час
            TimeTemp hour_mean;
            hour_mean.time = hour_start_time; // используем время начала периода
            hour_mean.temp = hour_sum / hour_cnt;

            // Пишем в лог средних по часу, храним только последний месяц
            appendRotate(TEMP_HOURLY_LOG, hour_mean, chrono::hours(24 * 30));

            hour_key = cur_hour;
            hour_sum = 0.0;
            hour_cnt = 0;
            hour_start_time = tt.time;
        }

        // по аналогии делаем измерения дня
        if (day_key == -1) {
            day_key = cur_day;
            day_start_time = tt.time;
        } else if (cur_day != day_key && day_cnt > 0) {
            TimeTemp day_mean;
            day_mean.time = day_start_time; // используем время начала периода
            day_mean.temp = day_sum / day_cnt;

            appendKeepCurrentYear(TEMP_DAILY_LOG, day_mean);

            day_key = cur_day;
            day_sum = 0.0;
            day_cnt = 0;
            day_start_time = tt.time;
        }

        // накапливаем значение
        hour_sum += tt.temp;
        hour_cnt++;

        day_sum += tt.temp;
        day_cnt++;
    }
};

// Глобальный указатель для обработки сигналов
static MeanAgg* g_aggregator = nullptr;
static volatile sig_atomic_t g_running = 1;

void signalHandler(int signum) {
    (void)signum;
    g_running = 0;
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Incorrect arguments number" << endl;
        cerr << "Usage example\n" <<
                "Windows: " << argv[0] << " COM4 9600\n" <<
                "POSIX:   " << argv[0] << " /dev/ttys003 9600\n";
        return 1;
    }

    const string PORT = argv[1];

    // Безопасный парсинг baud rate
    char* endptr = nullptr;
    const long baud_long = strtol(argv[2], &endptr, 10);
    if (endptr == argv[2] || *endptr != '\0' || baud_long <= 0 || baud_long > INT_MAX) {
        cerr << "Invalid baud rate: " << argv[2] << endl;
        return 1;
    }
    const int BAUD = static_cast<int>(baud_long);

    // Устанавливаем C локаль для корректного парсинга чисел с точкой
    setlocale(LC_NUMERIC, "C");

    // создание папки для логов
    std::error_code ec;
    std::filesystem::create_directories("log", ec);
    if (ec) {
        cerr << "Warning: Failed to create log directory: " << ec.message() << endl;
    }

    // Устанавливаем обработчики сигналов для корректного завершения
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        SerialPort port(PORT, BAUD);
        MeanAgg ma;
        g_aggregator = &ma;

        cerr << "Opened port: " << PORT << " @ " << BAUD << "\n";
        cerr << "Press Ctrl+C to stop and save accumulated data.\n";

        while (g_running) {
            string line;

            if (!port.readline(line)) continue;
            if (line.empty()) continue;

            try {
                double temp = stod(line);

                TimeTemp tt;
                tt.time = chrono::system_clock::now();
                tt.temp = temp;

                appendRotate(TEMP_LOG, tt, chrono::hours(24));
                ma.add(tt);

                cerr << "Read from port temperature: " << temp << " C\n";
            } catch (...) {
                cerr << "Invalid format line: '" << line << "'\n";
            }
        }

        // Сохраняем накопленные данные перед выходом
        ma.flush();
        cerr << "Data saved successfully.\n";

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}
