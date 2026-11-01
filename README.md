# AI-компаньон для ESP32-S3-Touch-AMOLED-1.75C

Цифровой питомец в круглом AMOLED-экране: два выразительных глаза, 18 эмоций со своими звуками, реакция на касания, движение и звуки, игры, часы, уведомления. В комплекте эмулятор, программа для ПК и приложение Android.

Вся логика и графика написаны один раз, в `core/libcompanion`, и работают и на плате, и в эмуляторе.

Что проверено, а что нет, описано в [PROJECT_STATUS.md](PROJECT_STATUS.md). **На настоящей плате проект пока не запускался.**

## Попробовать без платы

```bash
pip install -r emulator/requirements.txt
```

```bash
python emulator/main.py
```

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
| `assets/sounds` | 59 звуков, синтезированы `tools/gen_sounds.py` |
| `tools` | Генераторы звука и шрифта, сборка, симулятор устройства, AI-шлюз |
| `tests` | Автотесты |
| `documentation` | Документация |

## Документация

[Установка](documentation/INSTALLATION.md) · [Оборудование](documentation/HARDWARE.md) · [Архитектура](documentation/ARCHITECTURE.md) · [Эмоции](documentation/EMOTIONS.md) · [Эмулятор](documentation/EMULATOR.md) · [Android](documentation/ANDROID.md) · [ПК](documentation/DESKTOP.md) · [Протокол](protocol/protocol.md) · [Руководство пользователя](documentation/USER_MANUAL.md)
