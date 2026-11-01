# AI-компаньон для ESP32-S3-Touch-AMOLED-1.75C

Цифровой питомец в круглом AMOLED-экране: два выразительных глаза, 50 эмоций со своими звуками, реакция на касания, движение, звуки, время суток и погоду, 8 игр, включая лабиринт на гироскопе, часы из квадратиков, уведомления. В комплекте эмулятор, программа для ПК и приложение Android.

Вся логика и графика написаны один раз, в `core/libcompanion`, и работают и на плате, и в эмуляторе.

Что проверено, а что нет, описано в [PROJECT_STATUS.md](PROJECT_STATUS.md). **На настоящей плате проект пока не запускался.**

## Попробовать без платы

```bash
pip install -r emulator/requirements.txt
```

```bash
python emulator/main.py
```

На Windows можно просто дважды щёлкнуть `run_emulator.bat`. Не запускайте двойным щелчком сам `main.py`: Windows может взять другую версию Python. Первый запуск и запуск после изменения ядра занимают до минуты.

Нажмите **DEMO MODE**.

## Прошить плату

```bash
python tools/build_firmware.py upload
```

Нужен [PlatformIO](https://platformio.org). Подробности в [INSTALLATION.md](documentation/INSTALLATION.md).

## Структура

| Папка | Содержимое |
|---|---|
| `core/libcompanion` | Общее ядро на C++17: лицо, эмоции, питомец, экраны, игры, звук, протокол |
| `core/companion.py`, `core/capi` | Подключение ядра к Python |
| `firmware` | Прошивка: драйверы, BLE, Wi-Fi |
| `emulator` | Графический эмулятор с панелью тестирования |
| `desktop` | Программа для ПК |
| `android` | Приложение на Kotlin и Compose |
| `protocol` | Описание протокола |
| `assets/sounds` | 128 звуков, синтезированы `tools/gen_sounds.py` |
| `tools` | Генераторы звука и шрифта, сборка, симулятор устройства, AI-шлюз |
| `tests` | Автотесты |
| `documentation` | Документация |

## Документация

[Установка](documentation/INSTALLATION.md) · [Оборудование](documentation/HARDWARE.md) · [Архитектура](documentation/ARCHITECTURE.md) · [Эмоции](documentation/EMOTIONS.md) · [Эмулятор](documentation/EMULATOR.md) · [Android](documentation/ANDROID.md) · [ПК](documentation/DESKTOP.md) · [Протокол](protocol/protocol.md) · [Руководство пользователя](documentation/USER_MANUAL.md)
