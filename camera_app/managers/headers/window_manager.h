#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

/**
 * @brief Менеджер окна и контекста OpenGL
 * 
 * Управляет инициализацией GLFW, GLAD и жизненным циклом окна
 */
class WindowManager {
public:
    WindowManager();
    ~WindowManager();
    
    /**
     * @brief Инициализация GLFW и создание окна
     * @param width Ширина окна
     * @param height Высота окна
     * @param title Заголовок окна
     * @return true если успешно, false при ошибке
     */
    bool initialize(int width, int height, const std::string& title);
    
    /**
     * @brief Очистка ресурсов GLFW
     */
    void shutdown();
    
    /**
     * @brief Получить указатель на GLFW окно
     * @return Указатель на окно или nullptr если не инициализировано
     */
    GLFWwindow* getWindow() const { return window; }
    
    /**
     * @brief Проверить, должен ли быть закрыт
     * @return true если окно должно быть закрыто
     */
    bool shouldClose() const;
    
    /**
     * @brief Обменять буферы (swap buffers)
     */
    void swapBuffers();
    
    /**
     * @brief Обработать события окна
     */
    void pollEvents();
    
    /**
     * @brief Получить версию OpenGL
     * @return Строка с версией OpenGL
     */
    std::string getOpenGLVersion() const;
    
    /**
     * @brief Получить версию GLSL
     * @return Строка с версией GLSL
     */
    std::string getGLSLVersion() const;
    
    /**
     * @brief Проверить, инициализирован ли менеджер
     * @return true если инициализирован
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * @brief Получить текущее время в секундах (GLFW time)
     * @return Время в секундах с момента инициализации GLFW
     */
    double getTime() const;

private:
    GLFWwindow* window;
    bool initialized;
    
    /**
     * @brief Callback для ошибок GLFW
     */
    static void errorCallback(int error, const char* description);
};

#endif // WINDOW_MANAGER_H

