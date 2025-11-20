import argparse
import asyncio
import os
import time
import sys

import python_weather
import serial
from datetime import datetime

CITY = 'Vladivostok'
UPDATE_INTERVAL = 10


def gen_default_port() -> str | None:
    '''
    Генерирует порт для отправки данных
    :return:
        port(str) | None
    '''
    if sys.platform.startswith("win"):  # Windows
        return "COM3"
    if sys.platform.startswith("linux"):  # Linux
        return "/dev/ttyUSB0"
    if sys.platform.startswith("darwin"):  # macOS
        return "/dev/ttys002"
    return None


parser = argparse.ArgumentParser(
    description='Симулятор устройства считывания температуры на улице.(python-weather)'
)

parser.add_argument(
    '--port',
    type=str,
    default=gen_default_port(),
    help='Имя порта для вывода(при пустом значении ставится стандартный для ОС порт)'
)

parser.add_argument(
    "--baudrate",
    type=int,
    default=9600,
    help='Скорость порта(по умолчанию 9600)'
)

parser.add_argument(
    '--city',
    type=str,
    default=CITY,
    help='Город для получения температуры(по умолчанию Vladivostok)'
)

parser.add_argument(
    "--interval",
    type=int,
    default=UPDATE_INTERVAL,
    help="Интервал между обновлениями в секундах (по умолчанию 10)"
)

args = parser.parse_args()

PORT = args.port
BAUDRATE = args.baudrate
CITY = args.city
UPD_INTERVAL = args.interval

if PORT is None:
    print('PORT is NoneType')
    sys.exit(1)


async def weather_sender() -> None:
    async with python_weather.Client() as client:
        ser = serial.Serial(
            port=PORT,
            baudrate=BAUDRATE,
            timeout=1
        )

        print(f"[{datetime.now()}] SENDER OPEN PORT {PORT} @ {BAUDRATE}, city: {CITY}")

        try:
            while True:
                try:
                    weather = await client.get(CITY)
                    temperature = float(weather.temperature)
                except Exception as e:
                    print(f"[{datetime.now()}] PYTHON_WEATHER ERROR: {e}")
                    await asyncio.sleep(UPD_INTERVAL)
                    continue

                line = f"{temperature:.2f}\n"
                ser.write(line.encode("utf-8"))
                ser.flush()

                print(f"[{datetime.now()}] → {PORT}: {line.strip()} °C")
                await asyncio.sleep(UPD_INTERVAL)

        except KeyboardInterrupt:
            print("SENDER stop by Ctrl+C")
        finally:
            ser.close()
            print(f"PORT {PORT} CLOSED")


if __name__ == '__main__':
    if os.name == "nt":  # nt | posix
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    asyncio.run(weather_sender())
