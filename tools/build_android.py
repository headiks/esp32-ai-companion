"""Builds the Android app. The Android Gradle plugin refuses non-ASCII project paths, so such a project is built
from an ASCII mirror. Needs JAVA_HOME (JDK 17) and ANDROID_HOME, or the default per-user install locations.
Usage: python tools/build_android.py [gradle tasks, default: assembleDebug testDebugUnitTest]"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MIRROR = Path(os.environ.get("COMPANION_MIRROR", "C:/companion_build/mirror" if os.name == "nt" else "/tmp/companion_mirror")) / "android"
LOCAL = Path(os.environ.get("LOCALAPPDATA", ""))


def main():
    env = dict(os.environ)
    env.setdefault("JAVA_HOME", str(LOCAL / "Programs" / "jdk-17"))
    env.setdefault("ANDROID_HOME", str(LOCAL / "Android" / "Sdk"))
    src = ROOT / "android"
    cwd = src
    if not str(ROOT).isascii():
        shutil.copytree(src, MIRROR, dirs_exist_ok=True, ignore=shutil.ignore_patterns("build", ".gradle", "local.properties"))
        cwd = MIRROR
    gradlew = cwd / ("gradlew.bat" if os.name == "nt" else "gradlew")
    tasks = sys.argv[1:] or ["assembleDebug", "testDebugUnitTest"]
    code = subprocess.call([str(gradlew), "--no-daemon", "--console=plain"] + tasks, cwd=cwd, env=env)
    apk = cwd / "app/build/outputs/apk/debug/app-debug.apk"
    if code == 0 and apk.exists() and cwd != src:
        print("APK:", apk)
    sys.exit(code)


if __name__ == "__main__":
    main()
