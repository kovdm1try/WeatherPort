#include "SerialPort.h"

#include <stdexcept>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cerrno>
#endif

using namespace std;

SerialPort::SerialPort(const string &port, int baud) {
#ifdef _WIN32
        // На windows порты > 9 имеют название следующего формата "\\.\COM10"
        string full = "\\\\.\\" + port;

        // Открываем на считывание
        HANDLE h = CreateFileA(
            full.c_str(),
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        // Если не получилось открыть порт
        if (h == INVALID_HANDLE_VALUE) {
            throw runtime_error("Failed to open port " + port);
        }
        handle = h;

        // Переменная хранящая параметры порта
        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);

        // Если не получается получить текущие настройки порта
        if (!GetCommState(h, &dcb)) {
            CloseHandle(h);
            throw runtime_error("Failed to get params by GetCommState");
        }


        dcb.BaudRate =  baud; // скорость порта - должна быть как в Python
        dcb.ByteSize = 8; // количество битов в символе(по стандарту 8)
        dcb.Parity   = NOPARITY; // (нет бита четности) просто данные без лишней информации
        dcb.StopBits = ONESTOPBIT; // 1 стоп бит в конце данных

        // сеттим новые данные
        if (!SetCommState(h, &dcb)) {
            CloseHandle(h);
            throw runtime_error("SetCommState failed");
        }

        // Если нет данных кидаем timeouterror
        COMMTIMEOUTS to{};
        to.ReadIntervalTimeout = 50;
        to.ReadTotalTimeoutMultiplier = 10;
        to.ReadTotalTimeoutConstant = 50;
        SetCommTimeouts(h, &to);

#else
    fid = open(port.c_str(), O_RDONLY | O_NOCTTY);
    if (fid < 0) {
        throw runtime_error("Failed to open port " + port + ": " + strerror(errno));
    }

    // переменная хранящая параметры порта
    termios tio{};
    if (tcgetattr(fid, &tio) != 0) {
        close(fid);
        throw runtime_error("Failed to complete tcgetattr");
    }

    // Включаем вырой режим работы порта чтоб исключить лишние данные
    cfmakeraw(&tio);

    // Устанавливаем скорость порта
    speed_t speed;
    switch (baud) {
        case 9600: speed = B9600;
            break;
        case 19200: speed = B19200;
            break;
        case 38400: speed = B38400;
            break;
        case 57600: speed = B57600;
            break;
        case 115200: speed = B115200;
            break;
        default:
            close(fid);
            throw std::runtime_error("Unsupported baud value: " + std::to_string(baud));
    }

    tio.c_cflag |= (CLOCAL | CREAD); // разрешаем чтение

    // Пытаемся установить аттрибуты на порт
    if (tcsetattr(fid, TCSANOW, &tio) != 0) {
        close(fid);
        throw runtime_error("tcsetattr failed");
    }
#endif
}

// Деструктор порта
SerialPort::~SerialPort() {
#ifdef _WIN32
        if (handle != nullptr) {
            CloseHandle((HANDLE)handle);
            handle = nullptr;
        }
#else
    if (fid >= 0) {
        close(fid);
        fid = -1;
    }
#endif
}

// Чтение одной строки с порта
bool SerialPort::readline(string &output) {
    output.clear();
    char c;

    while (true) {
        int r = readByte(c);
        if (r <= 0) {
            // Данных больше нет
            return !output.empty();
        }

        if (c == '\n') {
            // Конец строки
            return true;
        }
        if (c == '\r') {
            continue;
        }
        output.push_back(c);
    }
}

#ifdef _WIN32
int SerialPort::readByte(char& c) {
    DWORD n = 0;
    if (!ReadFile((HANDLE)handle, &c, 1, &n, nullptr)) {
        return -1; // Не получилось считать байт
    }
    return (int)n; // 0|1
}
#else
int SerialPort::readByte(char &c) const {
    ssize_t n = ::read(fid, &c, 1);
    if (n < 0) return -1;
    return (int) n; // 0 или 1
}
#endif
