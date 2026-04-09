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
        CloseHandle(hFile);
        if(hMutex){
            CloseHandle(hMutex);
        }
        if(hEmpty){
            CloseHandle(hEmpty);
        }
        if(hFull){
            CloseHandle(hFull);
        }
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
            return 1;
        }
        readyEvents.push_back(hEvent);
    
        // Parameters
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        si.cb = sizeof(si);

        std::string cmd = "sender.exe " + filename + " " + std::to_string(i);
        
        if(!CreateProcess(NULL, cmd.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)){
            PrintError("CreateProcess failed for sender " + std::to_string(i));
            return 1;
        }

        processInfos.push_back(pi);
        CloseHandle(pi.hThread);
    }

    std::cout << "Waiting for all senders to be ready..." << std::endl;
}