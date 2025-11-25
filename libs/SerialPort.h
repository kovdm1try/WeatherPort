//
// Created by Дмитрий on 11/24/25.
//

#pragma once;

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <string>

using namespace std;

class SerialPort {
public:
    SerialPort(const string& port, int baud);

    // Деструктор
    ~SerialPort();

    bool readline(string& output);
private:
    // fid/handle - дескрипторы ресурсов откуда мы будем считывать данные(с порта)
    #ifdef _WIN32
        void *handle = nullptr;
        int readByte(char& c);
    #else
        int fid = -1;
        int readByte(char& c) const;
    #endif
};



#endif //SERIALPORT_H
