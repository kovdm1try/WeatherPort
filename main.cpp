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
#include <thread>
#include <mutex>
#include <atomic>

#include "libs/SerialPort.h"
#include "libs/httplib.h"
#include <sqlite3.h>

#ifdef _WIN32
    #include <windows.h>
#endif

using namespace std;

const string DB_PATH = "log/weather.db";

const int HTTP_PORT = 9847;

static atomic<bool> g_running{true};
static mutex g_db_mutex;
static sqlite3* g_db = nullptr;
static double g_current_temp = 0.0;
static chrono::system_clock::time_point g_last_reading_time;
static mutex g_temp_mutex;


struct TimeTemp {
    chrono::system_clock::time_point time;
    double temp{};
};

// преобразование time_point в строку вида YYYY/MM/DD HH:MM:SS
string TimePoint2String(const chrono::system_clock::time_point tp) {
    time_t t = chrono::system_clock::to_time_t(tp);
    tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    ostringstream oss;
    oss << put_time(&tm, "%Y/%m/%d %H:%M:%S");
    return oss.str();
}

// преобразование time_point в Unix timestamp
long long TimePoint2Unix(const chrono::system_clock::time_point tp) {
    return chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count();
}

// переобразование Unix timestamp в time_point
chrono::system_clock::time_point Unix2TimePoint(long long ts) {
    return chrono::system_clock::time_point(chrono::seconds(ts));
}

bool initDatabase() {
    error_code ec;
    filesystem::create_directories("log", ec);

    int rc = sqlite3_open(DB_PATH.c_str(), &g_db);
    if (rc != SQLITE_OK) {
        cerr << "Cannot open database: " << sqlite3_errmsg(g_db) << endl;
        return false;
    }

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS measurements (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            temperature REAL NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_measurements_ts ON measurements(timestamp);

        CREATE TABLE IF NOT EXISTS hourly_stats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            avg_temp REAL NOT NULL,
            count INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_hourly_ts ON hourly_stats(timestamp);

        CREATE TABLE IF NOT EXISTS daily_stats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            avg_temp REAL NOT NULL,
            count INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_daily_ts ON daily_stats(timestamp);
    )";

    char* errMsg = nullptr;
    rc = sqlite3_exec(g_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

void insertMeasurement(long long timestamp, double temp) {
    lock_guard<mutex> lock(g_db_mutex);

    const char* sql = "INSERT INTO measurements (timestamp, temperature) VALUES (?, ?)";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, timestamp);
        sqlite3_bind_double(stmt, 2, temp);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void insertHourlyStat(long long timestamp, double avg_temp, int count) {
    lock_guard<mutex> lock(g_db_mutex);

    const char* sql = "INSERT INTO hourly_stats (timestamp, avg_temp, count) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, timestamp);
        sqlite3_bind_double(stmt, 2, avg_temp);
        sqlite3_bind_int(stmt, 3, count);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void insertDailyStat(long long timestamp, double avg_temp, int count) {
    lock_guard<mutex> lock(g_db_mutex);

    const char* sql = "INSERT INTO daily_stats (timestamp, avg_temp, count) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, timestamp);
        sqlite3_bind_double(stmt, 2, avg_temp);
        sqlite3_bind_int(stmt, 3, count);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// удаление измерений
void cleanupOldMeasurements() {
    lock_guard<mutex> lock(g_db_mutex);

    auto cutoff = chrono::system_clock::now() - chrono::hours(24);
    long long cutoff_ts = TimePoint2Unix(cutoff);

    const char* sql = "DELETE FROM measurements WHERE timestamp < ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff_ts);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void cleanupOldHourlyStats() {
    lock_guard<mutex> lock(g_db_mutex);

    auto cutoff = chrono::system_clock::now() - chrono::hours(24 * 30);
    long long cutoff_ts = TimePoint2Unix(cutoff);

    const char* sql = "DELETE FROM hourly_stats WHERE timestamp < ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff_ts);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void cleanupOldDailyStats() {
    lock_guard<mutex> lock(g_db_mutex);

    auto now = chrono::system_clock::now();
    time_t t_now = chrono::system_clock::to_time_t(now);
    tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &t_now);
#else
    localtime_r(&t_now, &tm_now);
#endif

    tm tm_year_start = {0, 0, 0, 1, 0, tm_now.tm_year, 0, 0, -1};
    time_t year_start = mktime(&tm_year_start);

    const char* sql = "DELETE FROM daily_stats WHERE timestamp < ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, static_cast<long long>(year_start));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// Агрегатор средних значений
struct MeanAgg {
    long long hour_key = -1;
    double hour_sum = 0.0;
    long long hour_cnt = 0;
    chrono::system_clock::time_point hour_start_time;

    long long day_key = -1;
    double day_sum = 0.0;
    long long day_cnt = 0;
    chrono::system_clock::time_point day_start_time;

    static long long dayKey(const tm& tm_time) {
        return (tm_time.tm_year + 1900) * 10000LL
             + (tm_time.tm_mon + 1) * 100LL
             + tm_time.tm_mday;
    }

    static long long hourKey(const tm& tm_time) {
        return dayKey(tm_time) * 100LL + tm_time.tm_hour;
    }

    void flush() {
        if (hour_cnt > 0) {
            insertHourlyStat(TimePoint2Unix(hour_start_time),
                           hour_sum / hour_cnt,
                           static_cast<int>(hour_cnt));
        }
        if (day_cnt > 0) {
            insertDailyStat(TimePoint2Unix(day_start_time),
                          day_sum / day_cnt,
                          static_cast<int>(day_cnt));
        }
    }

    void add(const TimeTemp& tt) {
        time_t t = chrono::system_clock::to_time_t(tt.time);
        tm tm_entry{};
#ifdef _WIN32
        localtime_s(&tm_entry, &t);
#else
        localtime_r(&t, &tm_entry);
#endif

        const long long cur_day = dayKey(tm_entry);
        const long long cur_hour = hourKey(tm_entry);

        if (hour_key == -1) {
            hour_key = cur_hour;
            hour_start_time = tt.time;
        } else if (cur_hour != hour_key && hour_cnt > 0) {
            insertHourlyStat(TimePoint2Unix(hour_start_time),
                           hour_sum / hour_cnt,
                           static_cast<int>(hour_cnt));

            hour_key = cur_hour;
            hour_sum = 0.0;
            hour_cnt = 0;
            hour_start_time = tt.time;
        }

        if (day_key == -1) {
            day_key = cur_day;
            day_start_time = tt.time;
        } else if (cur_day != day_key && day_cnt > 0) {
            insertDailyStat(TimePoint2Unix(day_start_time),
                          day_sum / day_cnt,
                          static_cast<int>(day_cnt));

            day_key = cur_day;
            day_sum = 0.0;
            day_cnt = 0;
            day_start_time = tt.time;
        }

        hour_sum += tt.temp;
        hour_cnt++;
        day_sum += tt.temp;
        day_cnt++;
    }
};

static MeanAgg* g_aggregator = nullptr;

void signalHandler(int signum) {
    (void)signum;
    g_running = false;
}

string escapeJson(const string& s) {
    string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

void runHttpServer() {
    httplib::Server svr;
    auto setCorsHeaders = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };

    svr.Get("/api/current", [&setCorsHeaders](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);

        double temp;
        long long ts;
        {
            lock_guard<mutex> lock(g_temp_mutex);
            temp = g_current_temp;
            ts = TimePoint2Unix(g_last_reading_time);
        }

        ostringstream json;
        json << fixed << setprecision(2);
        json << "{\"temperature\":" << temp << ",\"timestamp\":" << ts << "}";

        res.set_content(json.str(), "application/json");
    });

    svr.Get("/api/measurements", [&setCorsHeaders](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);

        int hours = 24;
        if (req.has_param("hours")) {
            hours = static_cast<int>(strtol(req.get_param_value("hours").c_str(), nullptr, 10));
        }

        auto cutoff = chrono::system_clock::now() - chrono::hours(hours);
        long long cutoff_ts = TimePoint2Unix(cutoff);

        ostringstream json;
        json << fixed << setprecision(2);
        json << "[";

        {
            lock_guard<mutex> lock(g_db_mutex);

            const char* sql = "SELECT timestamp, temperature FROM measurements WHERE timestamp >= ? ORDER BY timestamp ASC";
            sqlite3_stmt* stmt;

            bool first = true;
            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, cutoff_ts);

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    if (!first) json << ",";
                    first = false;

                    long long ts = sqlite3_column_int64(stmt, 0);
                    double temp = sqlite3_column_double(stmt, 1);
                    json << "{\"timestamp\":" << ts << ",\"temperature\":" << temp << "}";
                }
                sqlite3_finalize(stmt);
            }
        }

        json << "]";
        res.set_content(json.str(), "application/json");
    });

    svr.Get("/api/hourly", [&setCorsHeaders](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);

        int days = 7;
        if (req.has_param("days")) {
            days = static_cast<int>(strtol(req.get_param_value("days").c_str(), nullptr, 10));
        }

        auto cutoff = chrono::system_clock::now() - chrono::hours(24 * days);
        long long cutoff_ts = TimePoint2Unix(cutoff);

        ostringstream json;
        json << fixed << setprecision(2);
        json << "[";

        {
            lock_guard<mutex> lock(g_db_mutex);

            const char* sql = "SELECT timestamp, avg_temp, count FROM hourly_stats WHERE timestamp >= ? ORDER BY timestamp ASC";
            sqlite3_stmt* stmt;

            bool first = true;
            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, cutoff_ts);

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    if (!first) json << ",";
                    first = false;

                    long long ts = sqlite3_column_int64(stmt, 0);
                    double temp = sqlite3_column_double(stmt, 1);
                    int count = sqlite3_column_int(stmt, 2);
                    json << "{\"timestamp\":" << ts << ",\"avg_temp\":" << temp << ",\"count\":" << count << "}";
                }
                sqlite3_finalize(stmt);
            }
        }

        json << "]";
        res.set_content(json.str(), "application/json");
    });

    svr.Get("/api/daily", [&setCorsHeaders](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);

        int days = 30;
        if (req.has_param("days")) {
            days = static_cast<int>(strtol(req.get_param_value("days").c_str(), nullptr, 10));
        }

        auto cutoff = chrono::system_clock::now() - chrono::hours(24 * days);
        long long cutoff_ts = TimePoint2Unix(cutoff);

        ostringstream json;
        json << fixed << setprecision(2);
        json << "[";

        {
            lock_guard<mutex> lock(g_db_mutex);

            const char* sql = "SELECT timestamp, avg_temp, count FROM daily_stats WHERE timestamp >= ? ORDER BY timestamp ASC";
            sqlite3_stmt* stmt;

            bool first = true;
            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, cutoff_ts);

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    if (!first) json << ",";
                    first = false;

                    long long ts = sqlite3_column_int64(stmt, 0);
                    double temp = sqlite3_column_double(stmt, 1);
                    int count = sqlite3_column_int(stmt, 2);
                    json << "{\"timestamp\":" << ts << ",\"avg_temp\":" << temp << ",\"count\":" << count << "}";
                }
                sqlite3_finalize(stmt);
            }
        }

        json << "]";
        res.set_content(json.str(), "application/json");
    });

    svr.Get("/api/stats", [&setCorsHeaders](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);

        ostringstream json;
        json << fixed << setprecision(2);

        {
            lock_guard<mutex> lock(g_db_mutex);

            double min_temp = 0, max_temp = 0, avg_temp = 0;
            int count = 0;

            const char* sql = "SELECT MIN(temperature), MAX(temperature), AVG(temperature), COUNT(*) FROM measurements";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    min_temp = sqlite3_column_double(stmt, 0);
                    max_temp = sqlite3_column_double(stmt, 1);
                    avg_temp = sqlite3_column_double(stmt, 2);
                    count = sqlite3_column_int(stmt, 3);
                }
                sqlite3_finalize(stmt);
            }

            json << "{\"min\":" << min_temp
                 << ",\"max\":" << max_temp
                 << ",\"avg\":" << avg_temp
                 << ",\"count\":" << count << "}";
        }

        res.set_content(json.str(), "application/json");
    });

    svr.Options(".*", [&setCorsHeaders](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res);
        res.set_content("", "text/plain");
    });

    cerr << "HTTP server starting on port " << HTTP_PORT << endl;
    svr.listen("0.0.0.0", HTTP_PORT);
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

    char* endptr = nullptr;
    const long baud_long = strtol(argv[2], &endptr, 10);
    if (endptr == argv[2] || *endptr != '\0' || baud_long <= 0 || baud_long > INT_MAX) {
        cerr << "Invalid baud rate: " << argv[2] << endl;
        return 1;
    }
    const int BAUD = static_cast<int>(baud_long);

    setlocale(LC_NUMERIC, "C");

    if (!initDatabase()) {
        cerr << "Failed to initialize database" << endl;
        return 1;
    }

    cleanupOldMeasurements();
    cleanupOldHourlyStats();
    cleanupOldDailyStats();

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    thread httpThread(runHttpServer);
    httpThread.detach();

    try {
        SerialPort port(PORT, BAUD);
        MeanAgg ma;
        g_aggregator = &ma;

        cerr << "Opened port: " << PORT << " @ " << BAUD << "\n";
        cerr << "HTTP server running at http://localhost:" << HTTP_PORT << "\n";
        cerr << "Press Ctrl+C to stop and save data.\n";

        while (g_running) {
            string line;

            if (!port.readline(line)) continue;
            if (line.empty()) continue;

            try {
                double temp = stod(line);

                TimeTemp tt;
                tt.time = chrono::system_clock::now();
                tt.temp = temp;

                insertMeasurement(TimePoint2Unix(tt.time), temp);

                {
                    lock_guard<mutex> lock(g_temp_mutex);
                    g_current_temp = temp;
                    g_last_reading_time = tt.time;
                }

                ma.add(tt);

                cerr << "Read from port temperature: " << temp << " C\n";
            } catch (...) {
                cerr << "Invalid format line: '" << line << "'\n";
            }
        }

        ma.flush();
        cerr << "Data saved.\n";

    } catch (const exception &e) {
        cerr << e.what() << endl;
        if (g_db) sqlite3_close(g_db);
        return 1;
    }

    if (g_db) sqlite3_close(g_db);
    return 0;
}
