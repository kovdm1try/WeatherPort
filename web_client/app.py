#!/usr/bin/env python3
from flask import Flask, render_template, jsonify, request
import requests
from datetime import datetime

app = Flask(__name__)

API_BASE_URL = "http://localhost:9847"


def format_timestamp(ts):
    return datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/current')
def get_current():
    try:
        resp = requests.get(f"{API_BASE_URL}/api/current", timeout=5)
        data = resp.json()
        data['timestamp_formatted'] = format_timestamp(data['timestamp']) if data['timestamp'] > 0 else 'N/A'
        return jsonify(data)
    except Exception as e:
        return jsonify({'error': str(e), 'temperature': 0, 'timestamp': 0}), 500


@app.route('/api/measurements')
def get_measurements():
    try:
        hours = request.args.get('hours', 24, type=int)
        resp = requests.get(f"{API_BASE_URL}/api/measurements", params={'hours': hours}, timeout=10)
        return jsonify(resp.json())
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/hourly')
def get_hourly():
    try:
        days = request.args.get('days', 7, type=int)
        resp = requests.get(f"{API_BASE_URL}/api/hourly", params={'days': days}, timeout=10)
        return jsonify(resp.json())
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/daily')
def get_daily():
    try:
        days = request.args.get('days', 30, type=int)
        resp = requests.get(f"{API_BASE_URL}/api/daily", params={'days': days}, timeout=10)
        return jsonify(resp.json())
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/stats')
def get_stats():
    try:
        resp = requests.get(f"{API_BASE_URL}/api/stats", timeout=5)
        return jsonify(resp.json())
    except Exception as e:
        return jsonify({'error': str(e)}), 500


if __name__ == '__main__':
    print("WeatherPort Web Client")
    print("=" * 40)
    print(f"API Server: {API_BASE_URL}")
    print("Web Client: http://localhost:9848")
    print("=" * 40)
    app.run(host='0.0.0.0', port=9848, debug=True)
