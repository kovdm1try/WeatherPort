# WeatherPort
## Задание

```
Есть устройство, которое публикует по серийному порту или интерфейсу USB 
текущую температуру окружающей среды. Необходимо написать на C\C++ кроссплатформенную программу, 
которая считывает информацию с порта и записывает ее в лог-файл. Каждый час программа считает 
среднюю температуру за час и записывает ее в другой лог-файл. 
Средняя температура за день записывается в 3й лог-файл. Лог файл со всеми измерениями должен 
хранить только измерения за последние 24 часа, лог-файл со средней температурой за час должен 
хранить только измерения за последний месяц, лог файл со средней дневной температурой накапливает 
информацию за текущий год.
```

## Структура проекта
```
WeatherPort/
├── lib/                 # Папка библиотеки
│   ├── SerialPort.h     # Заголовочный файл класса для работы с SerialPort
│   └── SerialPort.cpp   # Код класса SerialPort
├── sender.py            # Программа Python эмулирующая устройство для определения температуры
├── main.cpp             # Программа C++
├── CMakeLists.txt       # Файл для сборки проекта с помощью CMake
├── requirements.txt     # Зависимости для Python
└── README.md            # Документация
```

## Запуск sender
`Для работы с портами на MacOS можно создать виртуальный порт через socat`
```bash
brew install socat

# Создание портов
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Пример вывода
2025/11/20 21:07:35 socat[42482] N PTY is /dev/ttys002
2025/11/20 21:07:35 socat[42482] N PTY is /dev/ttys003
2025/11/20 21:07:35 socat[42482] N starting data transfer loop with FDs [5,5] and [7,7]
```
Далее можем запустить программу подключившись к какому-либо из портов
```bash
python weather_device.py --port /dev/ttys003
```

[Help] sender
```bash
usage: sender.py [-h] [--port PORT] [--baudrate BAUDRATE] [--city CITY] [--interval INTERVAL]

Симулятор устройства считывания температуры на улице.(python-weather)

options:
  -h, --help           show this help message and exit
  --port PORT          Имя порта для вывода(при пустом значении ставится стандартный для ОС порт)
  --baudrate BAUDRATE  Скорость порта(по умолчанию 9600)
  --city CITY          Город для получения температуры(по умолчанию Vladivostok)
  --interval INTERVAL  Интервал между обновлениями в секундах (по умолчанию 10)
```

## Запуск логера
```bash
# Сборка проекта
mkdir -p build
cd build
cmake ..
cmake --build .

# Запуск
./logger /dev/ttys002(порт) 9600(скорость)
```

## Запуск проекта целиком
1) Запуск виртуальных портов(Если на POSIX)
```bash
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

2) Запуск sender для получения температуры
```bash
source .venv/bin/activate
python sender.py --interval 30 --port /dev/ttys003 --city Vladivostok # Пример аргументов
```

3) Запуск логера
```bash
mkdir -p build
cd build
cmake ..
cmake --build .

# Запуск (Если через socat, то пишем второй, отличный от sender'а порт)
./logger /dev/ttys002 9600
```