# Инструкция по установке и сборке

## Шаг 1: Установка vcpkg (менеджер пакетов C++)

### Если vcpkg уже установлен:
Пропустите этот шаг и перейдите к Шагу 2.

### Если vcpkg не установлен:

1. Откройте **PowerShell** или **Command Prompt** от имени администратора

2. Клонируйте vcpkg в удобное место (например, `C:\`):
```cmd
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

3. Добавьте vcpkg в переменную окружения (опционально, но рекомендуется):
```cmd
setx VCPKG_ROOT "C:\vcpkg"
```

4. Перезапустите командную строку, чтобы применились изменения

## Шаг 2: Установка зависимостей через vcpkg

В командной строке выполните:

```cmd
cd C:\vcpkg
vcpkg install glfw3:x64-windows
vcpkg install imgui[glfw-binding,opengl3-binding]:x64-windows
vcpkg install glad:x64-windows
```

**Примечание:** Установка займет 5-15 минут в зависимости от скорости интернета.

## Шаг 3: Проверка OpenCV

Убедитесь, что OpenCV установлен в `C:\opencv\build`. Проверьте наличие файла:
```
C:\opencv\build\OpenCVConfig.cmake
```

Если файла нет, переустановите OpenCV или измените путь в `CMakeLists.txt`.

## Шаг 4: Установка Visual Studio

Если у вас еще не установлен Visual Studio:

1. Скачайте **Visual Studio 2022 Community** (бесплатно):
   https://visualstudio.microsoft.com/vs/community/

2. При установке выберите компонент:
   - **Desktop development with C++**

## Шаг 5: Сборка проекта

### Вариант A: Автоматическая сборка (рекомендуется)

Просто запустите:
```cmd
build.bat
```

### Вариант B: Ручная сборка

```cmd
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Шаг 6: Запуск приложения

После успешной сборки:

```cmd
cd build\Release
camera_app.exe
```

**Примечания:**
- При первом запуске Windows может запросить разрешение на доступ к камере
- Если появится ошибка об отсутствующих DLL, скопируйте OpenCV DLL из `C:\opencv\build\x64\vc16\bin` в папку с .exe

## Решение распространенных проблем

### Проблема: "CMake Error: Could not find OpenCV"

**Решение:**
1. Проверьте, что OpenCV установлен в `C:\opencv\build`
2. Если OpenCV в другом месте, измените путь в `CMakeLists.txt`:
   ```cmake
   set(OpenCV_DIR "C:/путь/к/opencv/build" CACHE PATH "Path to OpenCV")
   ```

### Проблема: "Cannot find vcpkg toolchain file"

**Решение:**
1. Убедитесь, что vcpkg установлен
2. Установите переменную окружения:
   ```cmd
   setx VCPKG_ROOT "C:\путь\к\vcpkg"
   ```
3. Перезапустите командную строку

### Проблема: "LINK : fatal error LNK1104: cannot open file 'opencv_world4XX.lib'"

**Решение:**
Версия OpenCV не соответствует ожидаемой. Убедитесь, что используете OpenCV 4.x

### Проблема: "Failed to open camera"

**Решение:**
1. Убедитесь, что камера подключена
2. Закройте другие приложения, использующие камеру (Skype, Teams, Zoom)
3. Проверьте разрешения Windows:
   - Settings → Privacy → Camera → Allow apps to access your camera

### Проблема: Приложение запускается, но черный экран

**Решение:**
1. Проверьте вывод в консоли - там будет информация об ошибках
2. Если используется CPU метод и он медленный, уменьшите разрешение камеры

## Настройка для разработки

Если вы хотите разрабатывать проект в Visual Studio:

1. Откройте Visual Studio
2. File → Open → CMake...
3. Выберите `CMakeLists.txt` из директории проекта
4. Visual Studio автоматически настроит проект
5. Нажмите F5 для запуска с отладкой

## Проверка версий

Для проверки установленных зависимостей:

```cmd
REM Проверка vcpkg
vcpkg list

REM Проверка CMake
cmake --version

REM Проверка Visual Studio
where cl.exe
```

## Дополнительная информация

- **OpenCV документация:** https://docs.opencv.org/
- **ImGui репозиторий:** https://github.com/ocornut/imgui
- **GLFW документация:** https://www.glfw.org/documentation.html
- **vcpkg документация:** https://vcpkg.io/

---

**Удачи! Если возникнут проблемы, проверьте README.md для дополнительной информации.**

