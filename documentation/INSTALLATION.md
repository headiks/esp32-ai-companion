# Установка и сборка

## Python: эмулятор, ПК, тесты

Нужен Python 3.10 или новее.

```bash
pip install -r emulator/requirements.txt
```

Ядро на C++ собирается само при первом запуске. Берётся системный компилятор (`c++`, `g++`, `clang++`), а если его нет — `ziglang` из `requirements.txt`. Ручная сборка:

```bash
python tools/build_core.py
```

Тесты:

```bash
python -m pytest tests -q
```

## Прошивка

Нужен PlatformIO Core:

```bash
pip install platformio
```

Сборка:

```bash
python tools/build_firmware.py
```

Прошивка (порт определяется сам; если нет, добавьте `--upload-port COM5`):

```bash
python tools/build_firmware.py upload
```

Если плата не входит в режим загрузки: удерживайте BOOT, нажмите и отпустите PWR или переподключите USB, отпустите BOOT.

Скрипт нужен, потому что компилятор ESP32 на Windows не читает пути с русскими буквами. При таком пути сборка идёт из копии в `C:\companion_build\mirror`. Если путь проекта только на латинице, можно просто выполнить `pio run` в `firmware/`.

Версии библиотек закреплены в `firmware/platformio.ini`. SensorLib и XPowersLib те же, что в примерах Waveshare, GFX новее: 1.6.4 не собирается на Core 3.3.12. Первая сборка скачивает около 2 ГБ и идёт 20–30 минут.

## Перегенерация ресурсов

Менять звуки:

```bash
python tools/gen_sounds.py
```

Менять шрифт, нужны `matplotlib` и `pillow`:

```bash
python tools/gen_font.py
```

После изменения звуков пересоберите прошивку: `assets/sounds.pack` встраивается в неё.

## Android

Смотрите [ANDROID.md](ANDROID.md).

## AI-сервер, по желанию

```bash
python tools/ai_gateway.py
```

Без настроек отвечает по простым правилам. Для настоящей модели задайте переменные окружения `AI_BASE_URL`, `AI_API_KEY`, `AI_MODEL`, для голоса ещё `AI_STT_MODEL`, `AI_TTS_MODEL`. Подходит любой сервис с API, совместимым с OpenAI.

На устройстве: настройте Wi-Fi командой `wifi_config` и укажите адрес сервера в настройке `ai_url`, например из вкладки «Настройки» программы для ПК.
