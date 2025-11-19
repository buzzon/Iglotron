#include "settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

bool SettingsManager::loadSettings(const std::string& filename, AppSettings& settings) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "Settings file not found: " << filename << ", creating default..." << std::endl;
            createDefaultSettings(filename);
            return loadSettings(filename, settings); // Retry
        }
        
        json j;
        file >> j;
        
        // Frangi parameters
        if (j.contains("frangi")) {
            auto& frangi = j["frangi"];
            if (frangi.contains("sigma")) settings.sigma = frangi["sigma"];
            if (frangi.contains("beta")) settings.beta = frangi["beta"];
            if (frangi.contains("c")) settings.c = frangi["c"];
            if (frangi.contains("displayStage")) settings.displayStage = frangi["displayStage"];
            if (frangi.contains("invertEnabled")) settings.invertEnabled = frangi["invertEnabled"];
            if (frangi.contains("segmentationThreshold")) 
                settings.segmentationThreshold = frangi["segmentationThreshold"];
        }
        
        // Preprocessing parameters
        if (j.contains("preprocessing")) {
            auto& prep = j["preprocessing"];
            
            if (prep.contains("globalContrast")) {
                auto& gc = prep["globalContrast"];
                if (gc.contains("enabled")) settings.globalContrastEnabled = gc["enabled"];
                if (gc.contains("brightness")) settings.globalBrightness = gc["brightness"];
                if (gc.contains("contrast")) settings.globalContrast = gc["contrast"];
            }
            
            if (prep.contains("clahe")) {
                auto& clahe = prep["clahe"];
                if (clahe.contains("enabled")) settings.claheEnabled = clahe["enabled"];
                if (clahe.contains("maxIterations")) settings.claheMaxIterations = clahe["maxIterations"];
                if (clahe.contains("targetContrast")) settings.claheTargetContrast = clahe["targetContrast"];
            }
        }
        
        // Camera parameters
        if (j.contains("camera")) {
            auto& camera = j["camera"];
            if (camera.contains("selectedIndex")) settings.selectedCameraIndex = camera["selectedIndex"];
        }
        
        std::cout << "Settings loaded successfully from: " << filename << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading settings: " << e.what() << std::endl;
        return false;
    }
}

bool SettingsManager::saveSettings(const std::string& filename, const AppSettings& settings) {
    try {
        json j;
        
        // Frangi parameters
        j["frangi"] = {
            {"sigma", settings.sigma},
            {"beta", settings.beta},
            {"c", settings.c},
            {"displayStage", settings.displayStage},
            {"invertEnabled", settings.invertEnabled},
            {"segmentationThreshold", settings.segmentationThreshold}
        };
        
        // Preprocessing parameters
        j["preprocessing"] = {
            {"globalContrast", {
                {"enabled", settings.globalContrastEnabled},
                {"brightness", settings.globalBrightness},
                {"contrast", settings.globalContrast}
            }},
            {"clahe", {
                {"enabled", settings.claheEnabled},
                {"maxIterations", settings.claheMaxIterations},
                {"targetContrast", settings.claheTargetContrast}
            }}
        };
        
        // Camera parameters
        j["camera"] = {
            {"selectedIndex", settings.selectedCameraIndex}
        };
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return false;
        }
        
        file << j.dump(4); // Pretty print with 4 spaces
        
        std::cout << "Settings saved successfully to: " << filename << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving settings: " << e.what() << std::endl;
        return false;
    }
}

bool SettingsManager::createDefaultSettings(const std::string& filename) {
    AppSettings defaultSettings;
    return saveSettings(filename, defaultSettings);
}

