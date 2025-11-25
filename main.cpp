#include <bits/stdc++.h>

#include "libs/SerialPort.h"

#ifdef _WIN32
    #include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cerrno>
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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Incorrect arguments number" << endl;
        cerr << "Usage example\n" <<
                "Windows " << argv[0] << " COM4 9600\n" <<
                "POSIX " << argv[0] << " dev/ttys003 9600\n";
        return 1;
    }

    const string PORT = argv[1];
    // Скорости в этом скрипте и запущенном симмуляторе python должны совпадать
    const int BAUD = atoi(argv[2]);

    try {
        SerialPort port(PORT, BAUD);
        cerr << "Opened port: " << PORT << " @ " << BAUD << "\n";

        while (true) {
            string line;

            if (!port.readline(line)) continue;
            if (line.empty()) continue;

            try {
                double temp = stod(line);

                TimeTemp tt;
                tt.time = chrono::system_clock::now();
                tt.temp = temp;

                cerr << "Read from port tempreture: " << temp << " C\n";
            } catch (...) {
                cerr << "Invalid format line: '" << line << "'\n";
            }
        }

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}
