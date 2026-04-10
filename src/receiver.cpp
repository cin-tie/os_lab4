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
    std::cout << "\n========== RECEIVER MENU ==========" << std::endl;
    std::cout << "/message - Read message from queue" << std::endl;
    std::cout << "/exit    - Exit receiver process" << std::endl;
    std::cout << "/help    - Show this menu" << std::endl;
    std::cout << "===================================" << std::endl;
}

// Read message from queue
bool ReadMessage(HANDLE hFile, HANDLE hMutex, HANDLE hFull, HANDLE hEmpty){
    // Wait for message to be available
    std::cout << "Waiting for message..." << std::endl;
    DWORD waitResult = WaitForSingleObject(hFull, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        PrintError("WaitForSingleObject (hFull) failed");
        return false;
    }
    
    if (waitResult == WAIT_OBJECT_0) {
        // Capture mutex to access file
        waitResult = WaitForSingleObject(hMutex, INFINITE);

        if (waitResult == WAIT_FAILED) {
            PrintError("WaitForSingleObject (hMutex) failed");
            ReleaseSemaphore(hFull, 1, NULL); // Return semaphore
            return false;
        }

        FileHeader header;
        DWORD bytesRead;

        // Reading fileHeader
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        if(!ReadFile(hFile, &header, sizeof(header), &bytesRead, NULL)){
            PrintError("ReadFile header error");
            ReleaseMutex(hMutex);
            ReleaseSemaphore(hFull, 1, NULL);
            return false;
        }

        if(header.count > 0){
            // Position of message
            LONG pos = sizeof(header) + header.head * sizeof(Message);
            SetFilePointer(hFile, pos, NULL, FILE_BEGIN);
            
            // Reading message
            Message msg;
            if(ReadFile(hFile, &msg, sizeof(msg), &bytesRead, NULL)){
                std::cout << "\n[RECEIVED] Message from queue: \"" << msg.text << "\"" << std::endl;

                // Update header
                header.head = (header.head + 1) % header.capacity;
                header.count--;

                // Write updated header
                SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                DWORD bytesWritten;
                if (!WriteFile(hFile, &header, sizeof(header), &bytesWritten, NULL)) {
                    PrintError("WriteFile header failed");
                    ReleaseMutex(hMutex);
                    ReleaseSemaphore(hFull, 1, NULL);
                    return false;
                }

                FlushFileBuffers(hFile);

                // Releasing semaphore for FIFO
                ReleaseSemaphore(hEmpty, 1, NULL);
                ReleaseMutex(hMutex);
                return true;
            }
            else{
                PrintError("ReadFile message failed");
                ReleaseMutex(hMutex);
                ReleaseSemaphore(hFull, 1, NULL);
                return false;
            }
        }
        else{
            // No messages(race condition with semaphore???)
            ReleaseMutex(hMutex);
            ReleaseSemaphore(hFull, 1, NULL);
            std::cout << "No messages available (race condition)" << std::endl;
            return false;
        }
    }

    return false;
}

int main() {
    std::string filename;
    int capacity;
    int senderCount;

    std::cout << "========== RECEIVER PROCESS ==========" << std::endl;
    std::cout << "Enter binary file name: ";
    std::cin >> filename;

    std::cout << "Enter queue capacity (number of messages): ";
    std::cin >> capacity;
    
    if (capacity <= 0) {
        std::cout << "Error: Capacity must be positive!" << std::endl;
        return 1;
    }

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

    std::cout << "Binary file created successfully with " << capacity << " slots." << std::endl;

    std::cout << "Enter number of Sender processes: ";
    std::cin >> senderCount;
    
    if (senderCount <= 0) {
        std::cout << "Error: Sender count must be positive!" << std::endl;
        CloseHandle(hFile);
        return 1;
    }

    // Creating synchronization objects
    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
    HANDLE hEmpty = CreateSemaphore(NULL, capacity, capacity, EMPTY_SEM_NAME);
    HANDLE hFull = CreateSemaphore(NULL, 0, capacity, FULL_SEM_NAME);

    if (!hMutex || !hEmpty || !hFull) {
        PrintError("Synchronization objects creation failed");
        CloseHandles({hFile, hMutex, hEmpty, hFull});
        std::cout << "All handles and processes closed\nShutting down..." << std::endl;
        return 1;
    }

    std::vector<HANDLE> readyEvents;
    std::vector<PROCESS_INFORMATION> processInfos;
    readyEvents.reserve(senderCount);
    processInfos.reserve(senderCount);

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
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
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
        CloseHandle(pi.hThread); // Thread handle not needed
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

    // Running main command loop
    bool running = true;
    DisplayMenu();
    while (running){

        std::cout << "\nReceiver> ";
        std::string command;
        std::cin >> command;
        
        if(command.compare("/message") == 0){
            if(ReadMessage(hFile, hMutex, hFull, hEmpty)){
                continue;
            }   
            else{
                std::cout << "Failed to read message" << std::endl;
            }
        }
        else if(command.compare("/exit") == 0){
            std::cout << "\nShutting down Receiver..." << std::endl;
            running = false;
        } else if(command.compare("/help") == 0){
            DisplayMenu();
        }
        else {
            std::cout << "Unknown command\nUse /help to see available" << std::endl;
        }
    }
    
    // Closing handles and finishing
    CloseHandles({hFile, hMutex, hEmpty, hFull});
    CloseHandles(readyEvents);
    CloseHandles(processInfos);
    std::cout << "All handles closed. Receiver terminated." << std::endl;
    return 0;
}