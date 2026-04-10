#pragma once

#include <windows.h>
#include <iostream>
#include <string>

// Colors for console
enum class LogColor {
    DEFAULT_COLOR = 7,
    RECEIVER_COLOR = 11,
    SENDER_COLOR = 9,
    ERROR_COLOR = 12,
    SUCCESS_COLOR = 10,
    WARNING_COLOR = 14,
    INFO_COLOR = 8
};

// Non sync safe console class
class ConsoleLogger{
private: 
    HANDLE consoleHandle;
    WORD defaultAttributes = 7;

public:
    // Initialize console(critical section)
    void Initialize(){
        consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

        // Save console attributes
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(consoleHandle, &csbi)) {
            defaultAttributes = csbi.wAttributes;
        }
        
    }    

    // Delete console(critical section)
    void Cleanup(){
        ResetColor();
    }

    // Set color
    void SetColor(LogColor color){
        if(consoleHandle != INVALID_HANDLE_VALUE){
            if(!SetConsoleTextAttribute(consoleHandle, static_cast<WORD>(color))){
                std::cerr << "CONSOLE ERROR" << std::to_string(GetLastError());
            }
        }
    }
    
    // Reset color
    void ResetColor() {
        if (consoleHandle != INVALID_HANDLE_VALUE) {
            if(!SetConsoleTextAttribute(consoleHandle, defaultAttributes)){
                std::cerr << "CONSOLE ERROR" << std::to_string(GetLastError());
            }
        }
    }

    // Output with color
    template<typename T>
    void Log(LogColor color, const T& message, bool addNewLine = true){

        SetColor(color);
        std::cout << message;
        if(addNewLine){
            std::cout << std::endl;
        }

        ResetColor();
    }

    void Receiver(const std::string& msg, bool addNewLine = true) {
        Log(LogColor::RECEIVER_COLOR, "[RECEIVER] " + msg, addNewLine);
    }

    void Sender(int id, const std::string& msg, bool addNewLine = true) {
        Log(LogColor::SENDER_COLOR, "[SENDER " + std::to_string(id) + "] " + msg, addNewLine);
    }

    void Error(const std::string& msg, bool addNewLine = true) {
        Log(LogColor::ERROR_COLOR, "[ERROR] " + msg, addNewLine);
    }

    void Success(const std::string& msg, bool addNewLine = true) {
        Log(LogColor::SUCCESS_COLOR, "[SUCCESS] " + msg, addNewLine);
    }

    void Warning(const std::string& msg, bool addNewLine = true) {
        Log(LogColor::WARNING_COLOR, "[WARNING] " + msg, addNewLine);
    }

    void Info(const std::string& msg, bool addNewLine = true) {
        Log(LogColor::INFO_COLOR, "[INFO] " + msg, addNewLine);
    }

    void Raw(LogColor color, const std::string& msg, bool addNewLine = true){
        Log(color, msg, addNewLine);
    }
};