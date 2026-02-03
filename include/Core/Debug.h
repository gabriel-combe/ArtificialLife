#pragma once

#include <iostream>
#include <string>

// Debug utility class - Unity-style logging
class Debug {
public:
    // Log messages
    static void Log(const std::string& message) {
        std::cout << "[LOG] " << message << std::endl;
    }
    
    static void LogWarning(const std::string& message) {
        std::cout << "[WARNING] " << message << std::endl;
    }
    
    static void LogError(const std::string& message) {
        std::cerr << "[ERROR] " << message << std::endl;
    }
    
    // Template versions for easy formatting
    template<typename... Args>
    static void Log(Args... args) {
        std::cout << "[LOG] ";
        (std::cout << ... << args) << std::endl;
    }
    
    template<typename... Args>
    static void LogWarning(Args... args) {
        std::cout << "[WARNING] ";
        (std::cout << ... << args) << std::endl;
    }
    
    template<typename... Args>
    static void LogError(Args... args) {
        std::cerr << "[ERROR] ";
        (std::cerr << ... << args) << std::endl;
    }
};