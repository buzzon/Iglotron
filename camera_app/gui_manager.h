#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include <opencv2/opencv.hpp>

// Forward declarations
struct GLFWwindow;
struct AppState;
typedef unsigned int GLuint;

/**
 * @brief Менеджер GUI для приложения
 * 
 * Управляет всем интерфейсом ImGui с единым окном и видеопотоками
 */
class GUIManager {
public:
    GUIManager();
    ~GUIManager();
    
    /**
     * @brief Инициализация ImGui
     * @param window Указатель на GLFW окно
     */
    void initialize(GLFWwindow* window);
    
    /**
     * @brief Очистка ресурсов ImGui
     */
    void shutdown();
    
    /**
     * @brief Отрисовка всего GUI (единое окно с настройками и видео)
     * @param state Состояние приложения
     */
    void render(AppState& state);
    
private:
    /**
     * @brief Отрисовка панели управления (левая колонка)
     */
    void renderControlPanel(AppState& state);
    
    /**
     * @brief Отрисовка видеопотоков (центр и право)
     */
    void renderVideoFeeds(AppState& state);
    
    /**
     * @brief Создание или обновление OpenGL текстуры из OpenCV Mat
     * @param texture Указатель на ID текстуры
     * @param image OpenCV изображение
     */
    void updateTexture(GLuint* texture, const cv::Mat& image);
    
    /**
     * @brief Отрисовка секции препроцессинга в панели управления
     */
    void renderPreprocessingSection(AppState& state);
    
    /**
     * @brief Отрисовка секции Frangi параметров в панели управления
     */
    void renderFrangiSection(AppState& state);
    
    bool m_initialized;
};

#endif // GUI_MANAGER_H

