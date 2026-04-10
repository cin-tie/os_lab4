#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include "../include/common.h"

ConsoleLogger logger;

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
    logger.Raw(LogColor::RECEIVER_COLOR, "\n========== RECEIVER MENU ==========");
    logger.Raw(LogColor::RECEIVER_COLOR, "/message - Read message from queue");
    logger.Raw(LogColor::RECEIVER_COLOR, "/exit    - Exit receiver process");
    logger.Raw(LogColor::RECEIVER_COLOR, "/help    - Show this menu");
    logger.Raw(LogColor::RECEIVER_COLOR, "===================================");
}

// Read message from queue
bool ReadMessage(HANDLE hFile, HANDLE hMutex, HANDLE hFull, HANDLE hEmpty, HANDLE hConsoleMutex){
    logger.Receiver("Waiting for message...");
    
    DWORD waitResult = WaitForSingleObject(hFull, 3000);

    if(waitResult == WAIT_TIMEOUT){
        logger.Receiver("Waiting timeout. Turn to ghostwaiting");
        ReleaseMutex(hConsoleMutex);
        waitResult = WaitForSingleObject(hFull, INFINITE);
        WaitForSingleObject(hConsoleMutex, INFINITE);
    }

    if (waitResult == WAIT_FAILED) {
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
                logger.Success("Message received: \"" + std::string(msg.text) + "\"");
    
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
            logger.Warning("No messages available (race condition)");
            return false;
        }
    }

    return false;
}

int main() {
    logger.Initialize();

    // Create console mutex
    HANDLE hConsoleMutex = CreateMutex(NULL, FALSE, CONSOLE_MUTEX_NAME);
    if (!hConsoleMutex) {
        std::cerr << "Failed to create console mutex" << std::endl;
        return 1;
    }

    std::string filename;
    int capacity;
    int senderCount;

    WaitForSingleObject(hConsoleMutex, INFINITE);

    logger.Receiver("========== RECEIVER PROCESS ==========");
    logger.Receiver("Enter binary file name: ", false);
    std::cin >> filename;
    ReleaseMutex(hConsoleMutex);
    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Receiver("Enter queue capacity (number of messages): ", false);
    std::cin >> capacity;
    ReleaseMutex(hConsoleMutex);
    
    if (capacity <= 0) {
        WaitForSingleObject(hConsoleMutex, INFINITE);
        logger.Error("Capacity must be positive!");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hConsoleMutex});
        logger.Cleanup();
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
        WaitForSingleObject(hConsoleMutex, INFINITE);
        PrintError("CreateFile error");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hConsoleMutex});
        logger.Cleanup();
        return 1;
    }

    InitializeFile(hFile, capacity);

    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Success("Binary file created successfully with " + std::to_string(capacity) + " slots.");
    ReleaseMutex(hConsoleMutex);

    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Receiver("Enter number of Sender processes: ", false);
    std::cin >> senderCount;
    ReleaseMutex(hConsoleMutex);
    
    if (senderCount <= 0) {
        WaitForSingleObject(hConsoleMutex, INFINITE);
        logger.Error("Sender count must be positive!");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hConsoleMutex});
        CloseHandles({hFile});
        logger.Cleanup();
        return 1;
    }

    // Creating synchronization objects
    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
    HANDLE hEmpty = CreateSemaphore(NULL, capacity, capacity, EMPTY_SEM_NAME);
    HANDLE hFull = CreateSemaphore(NULL, 0, capacity, FULL_SEM_NAME);

    if (!hMutex || !hEmpty || !hFull) {
        WaitForSingleObject(hConsoleMutex, INFINITE);
        PrintError("Synchronization objects creation failed");
        logger.Error("Shutting down...");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hFile, hMutex, hEmpty, hFull, hConsoleMutex});
        logger.Cleanup();
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
            WaitForSingleObject(hConsoleMutex, INFINITE);
            PrintError("CreateEvent failed");
            logger.Error("Shutting down...");
            ReleaseMutex(hConsoleMutex);
            CloseHandles({hFile, hMutex, hEmpty, hFull, hEvent, hConsoleMutex});
            CloseHandles(readyEvents);
            CloseHandles(processInfos);
            logger.Cleanup();
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
            WaitForSingleObject(hConsoleMutex, INFINITE);
            PrintError("CreateProcess failed for sender " + std::to_string(i));
            logger.Error("Shutting down...");
            ReleaseMutex(hConsoleMutex);
            CloseHandles({hFile, hMutex, hEmpty, hFull, hEvent, hConsoleMutex});
            CloseHandles(readyEvents);
            CloseHandles(processInfos);
            logger.Cleanup();
            return 1;
        }

        processInfos.push_back(pi);
        CloseHandle(pi.hThread); // Thread handle not needed
    }

    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Receiver("Waiting for all senders to be ready...");
    ReleaseMutex(hConsoleMutex);

    // Waiting for objects
    DWORD waitResult = WaitForMultipleObjects(senderCount, readyEvents.data(), TRUE, INFINITE);
    if (waitResult == WAIT_FAILED) {
        WaitForSingleObject(hConsoleMutex, INFINITE);
        PrintError("WaitForMultipleObjects failed");
        logger.Error("Shutting down...");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hFile, hMutex, hEmpty, hFull, hConsoleMutex});
        CloseHandles(readyEvents);
        CloseHandles(processInfos);
        logger.Cleanup();
        return 1;
    }
    
    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Success("All senders are ready!");
    logger.Receiver("Receiver is ready to work.");
    logger.Receiver("");
    ReleaseMutex(hConsoleMutex);

    // Running main command loop
    bool once = true;
    while (true){
        WaitForSingleObject(hConsoleMutex, INFINITE);
        if(once){
            DisplayMenu();
            once = false;
        }
        logger.Receiver("Receiver> ", false);
        
        std::string command;
        std::cin >> command;
        
        if(command.compare("/message") == 0){
            if(!ReadMessage(hFile, hMutex, hFull, hEmpty, hConsoleMutex)){
                logger.Error("Failed to read message");
            }
        }
        else if(command.compare("/exit") == 0){
            logger.Receiver("Shutting down Receiver...");
            break;
        } else if(command.compare("/help") == 0){
            DisplayMenu();
        }
        else {
            logger.Warning("Unknown command. Use /help to see available");
        }
        ReleaseMutex(hConsoleMutex);

    }
    
    // Closing handles and finishing
    CloseHandles(readyEvents);
    CloseHandles(processInfos);
    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Receiver("Receiver terminating...");
    ReleaseMutex(hConsoleMutex);
    CloseHandles({hFile, hMutex, hEmpty, hFull, hConsoleMutex});
    logger.Cleanup();
    return 0;
}