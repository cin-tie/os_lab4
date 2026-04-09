#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include "../include/common.h"

// Initialize empty binary file
void InitializeFile(HANDLE hFile, int capacity){
    FileHeader fileHeader;
    fileHeader.capacity = capacity;
    fileHeader.head = 0;
    fileHeader.tail = 0;
    fileHeader.count = 0;

    // Write header to file
    DWORD written;
    WriteFile(hFile, &fileHeader, sizeof(fileHeader), &written, NULL);

    // Fill file with empty messages
    Message empty;
    memset(empty.text, 0, MAX_MSG_SIZE);
    for(int i = 0; i < capacity; ++i){
        WriteFile(hFile, &empty, sizeof(empty), &written, NULL);    
    }
    FlushFileBuffers(hFile);
}

// Menu
inline void DisplayMenu(){
    std::cout << "\nCommands:" << std::endl;
    std::cout << "/message\t# Read message" << std::endl;
    std::cout << "/exit\t# Exit receiver process" << std::endl;
}

// Закрыть все дискрипторы
void CloseHandles(std::initializer_list<HANDLE> handles){
    for (HANDLE h : handles) {
        if (h != NULL && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
}
void CloseHandles(const std::vector<HANDLE>& handles) {
    for (HANDLE h : handles) {
        if (h != NULL && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
}
void CloseHandles(const std::vector<PROCESS_INFORMATION>& processInfos) {
    for (const auto& pi : processInfos) {
        if (pi.hProcess != NULL) {
            TerminateProcess(pi.hProcess, 0); 
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
        }
        if (pi.hThread != NULL) {
            CloseHandle(pi.hThread);
        }
    }
}

int main() {
    std::string filename;
    int capacity;
    int senderCount;

    std::cout << "File name: ";
    std::cin >> filename;

    std::cout << "Queue capacity: ";
    std::cin >> capacity;

    // Creating a file to write where
    HANDLE hFile = CreateFile(
        filename.c_str(), 
        GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, CREATE_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    // Error handling
    if(hFile == INVALID_HANDLE_VALUE){
        PrintError("CreateFile error");
        return 1;
    }

    InitializeFile(hFile, capacity);

    std::cout << "Sender count: ";
    std::cin >> senderCount;

    // Creating mutex and semaphores
    HANDLE hMutex = CreateMutexA(NULL, FALSE, MUTEX_NAME);
    HANDLE hEmpty = CreateSemaphoreA(NULL, capacity, capacity, EMPTY_SEM_NAME);
    HANDLE hFull = CreateSemaphoreA(NULL, 0, capacity, FULL_SEM_NAME);

    if (!hMutex || !hEmpty || !hFull) {
        PrintError("Synchronization objects creation failed");
        CloseHandles({hFile, hMutex, hEmpty, hFull});
        std::cout << "All handles and processes closed\nShutting down..." << std::endl;
        return 1;
    }

    std::vector<HANDLE> readyEvents;
    std::vector<PROCESS_INFORMATION> processInfos;

    // Sender running
    for(int i = 0; i < senderCount; ++i){
        std::string eventName = std::string(READY_EVENT_PREFIX) + std::to_string(i);

        // Creating an event
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, eventName.c_str());
        if (!hEvent) {
            PrintError("CreateEvent failed");
            CloseHandles({hFile, hMutex, hEmpty, hFull, hEvent});
            CloseHandles(readyEvents);
            CloseHandles(processInfos);
            std::cout << "All handles and processes closed\nShutting down..." << std::endl;
            return 1;
        }
        readyEvents.push_back(hEvent);
    
        // Parameters
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        si.cb = sizeof(si);

        std::string cmd = "sender.exe " + filename + " " + std::to_string(i);
        
        // Creating processes
        if(!CreateProcess(NULL, cmd.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)){
            PrintError("CreateProcess failed for sender " + std::to_string(i));
            CloseHandles({hFile, hMutex, hEmpty, hFull, hEvent});
            CloseHandles(readyEvents);
            CloseHandles(processInfos);
            std::cout << "All handles and processes closed\nShutting down..." << std::endl;
            return 1;
        }

        processInfos.push_back(pi);
        CloseHandle(pi.hThread);
    }

    std::cout << "Waiting for all senders to be ready..." << std::endl;

    // Waiting for objects
    DWORD waitResult = WaitForMultipleObjects(senderCount, readyEvents.data(), TRUE, INFINITE);
    if (waitResult == WAIT_FAILED) {
        PrintError("WaitForMultipleObjects failed");
        CloseHandles({hFile, hMutex, hEmpty, hFull});
        CloseHandles(readyEvents);
        CloseHandles(processInfos);
        std::cout << "All handles and processes closed\nShutting down..." << std::endl;
        return 1;
    }

    std::cout << "All senders are ready!" << std::endl;
    std::cout << "Receiver is ready to work." << std::endl;

    // Running main actions cycle
    bool running = true;
    while (running){
        DisplayMenu();

        std::string choice;
        std::cin >> choice;
        
        if(choice.compare("/message") == 0){
            
        }
        else if(choice.compare("/exit") == 0){
            running = false;
        } else{
            continue;
        }
    }
    
    // Closing handles and finishing
    CloseHandles({hFile, hMutex, hEmpty, hFull});
    CloseHandles(readyEvents);
    CloseHandles(processInfos);
    std::cout << "All handles and processes closed\nShutting down..." << std::endl;
    return 0;
}