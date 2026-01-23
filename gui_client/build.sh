#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

if [[ "$OSTYPE" == "darwin"* ]]; then
    if [ -d "/opt/homebrew/opt/qt" ]; then
        QT_PATH="/opt/homebrew/opt/qt"
    elif [ -d "/usr/local/opt/qt" ]; then
        QT_PATH="/usr/local/opt/qt"
    else
        echo "Qt не найден"
        exit 1
    fi
    CMAKE_PREFIX="-DCMAKE_PREFIX_PATH=$QT_PATH"
else
    CMAKE_PREFIX=""
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake $CMAKE_PREFIX "$@" ..

echo "=== Сборка ==="
cmake --build .

if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Запуск: $BUILD_DIR/WeatherGUI.app/Contents/MacOS/WeatherGUI"
else
    echo "Запуск: $BUILD_DIR/WeatherGUI"
fi
