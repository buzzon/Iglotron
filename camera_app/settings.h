#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

// Структура настроек приложения
struct AppSettings {
    // Параметры Frangi фильтра
    float sigma = 1.5f;
    float beta = 0.5f;
    float c = 15.0f;
    int displayStage = 8;
    bool invertEnabled = true;
    float segmentationThreshold = 0.01f;
    
    // Параметры препроцессинга
    bool globalContrastEnabled = false;
    float globalBrightness = 20.0f;
    float globalContrast = 3.0f;
    
    bool claheEnabled = false;
    int claheMaxIterations = 2;
    float claheTargetContrast = 0.3f;
    
    // Камера
    int selectedCameraIndex = 0;
    
    // Аппрувинг (injection window)
    bool approvalEnabled = false;
    int approvalMaskHeight = 100;
    int approvalMaskWidth = 200;
    float approvalThreshold = 0.5f;
    
    // Пути к файлам
    std::string settingsFile = "settings.json";
};

// Функции для работы с настройками
class SettingsManager {
public:
    // Загрузить настройки из файла
    static bool loadSettings(const std::string& filename, AppSettings& settings);
    
    // Сохранить настройки в файл
    static bool saveSettings(const std::string& filename, const AppSettings& settings);
    
    // Создать файл с дефолтными настройками
    static bool createDefaultSettings(const std::string& filename);
};

#endif // SETTINGS_H

