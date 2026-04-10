#include <windows.h>
#include <iostream>
#include <string>
#include "../include/common.h"

ConsoleLogger logger;

// Display sender menu
void DisplaySenderMenu(int id) {
    logger.Raw(LogColor::SENDER_COLOR, "\n========== SENDER " + std::to_string(id) + " MENU ==========");
    logger.Raw(LogColor::SENDER_COLOR, "/send   - Send message to receiver");
    logger.Raw(LogColor::SENDER_COLOR, "/exit   - Exit sender process");
    logger.Raw(LogColor::SENDER_COLOR, "/help   - Show this menu");
    logger.Raw(LogColor::SENDER_COLOR, "=====================================");
}

// Send message to queue
bool SendMessage(HANDLE hFile, HANDLE hMutex, HANDLE hEmpty, HANDLE hFull, int senderId, HANDLE hConsoleMutex){
    logger.Sender(senderId, "Enter message (max " + std::to_string(MAX_MSG_SIZE - 1) + " chars): ", false);
    
    std::string message;
    std::cin.ignore(); // Clear input buffer
    std::getline(std::cin, message);

    // Validate message
    if(message.empty()){
        logger.Error("Message cannot be empty!");
        return false;
    }

    if (message.length() >= MAX_MSG_SIZE) {
        logger.Error("Message too long! Maximum " + std::to_string(MAX_MSG_SIZE - 1) + " characters.");
        return false;
    }

    // Wait for empty slot in queue
    logger.Sender(senderId, "Waiting for free slot in queue...");
    
    DWORD waitResult = WaitForSingleObject(hEmpty, 3000);

    if(waitResult == WAIT_TIMEOUT){
        logger.Sender(senderId, "Waiting timeout. Turn to ghostwaiting");
        ReleaseMutex(hConsoleMutex);
        waitResult = WaitForSingleObject(hEmpty, INFINITE);
        WaitForSingleObject(hConsoleMutex, INFINITE);
    }

    if (waitResult == WAIT_FAILED) {
        PrintError("WaitForSingleObject (hEmpty) failed");
        return false;
    }

    // Capture mutex to access file
    waitResult = WaitForSingleObject(hMutex, INFINITE);
    if (waitResult == WAIT_FAILED) {
        PrintError("WaitForSingleObject (hMutex) failed");
        ReleaseSemaphore(hEmpty, 1, NULL);
        return false;
    }

    FileHeader fileHeader;
    DWORD bytesRead;

    // Read current header
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (!ReadFile(hFile, &fileHeader, sizeof(fileHeader), &bytesRead, NULL)) {
        PrintError("ReadFile header failed");
        ReleaseMutex(hMutex);
        ReleaseSemaphore(hEmpty, 1, NULL);
        return false;
    }

    // Check if queue is full (safety check)
    if (fileHeader.count >= fileHeader.capacity) {
        logger.Error("Queue is full!");
        ReleaseMutex(hMutex);
        ReleaseSemaphore(hEmpty, 1, NULL);
        return false;
    }

    // Position to write new message
    LONG pos = sizeof(fileHeader) + fileHeader.tail * sizeof(Message);
    SetFilePointer(hFile, pos, NULL, FILE_BEGIN);

    // Prepare and write message
    Message msg;
    memset(msg.text, 0, MAX_MSG_SIZE);
    strncpy(msg.text, message.c_str(), MAX_MSG_SIZE - 1);

    DWORD bytesWritten;
    if (!WriteFile(hFile, &msg, sizeof(msg), &bytesWritten, NULL)) {
        PrintError("WriteFile message failed");
        ReleaseMutex(hMutex);
        ReleaseSemaphore(hEmpty, 1, NULL);
        return false;
    }

    // Update header
    fileHeader.tail = (fileHeader.tail + 1) % fileHeader.capacity;
    fileHeader.count++;
    
    // Write updated header
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (!WriteFile(hFile, &fileHeader, sizeof(fileHeader), &bytesWritten, NULL)) {
        PrintError("WriteFile header failed");
        ReleaseMutex(hMutex);
        ReleaseSemaphore(hEmpty, 1, NULL);
        return false;
    }

    FlushFileBuffers(hFile);
    
    // Release resources
    ReleaseMutex(hMutex);
    ReleaseSemaphore(hFull, 1, NULL);
    
    logger.Success("Message sent successfully!");
    return true;
}

int main(int argc, char* argv[]){
    logger.Initialize();
    
    // Open console mutex (created by receiver)
    HANDLE hConsoleMutex = OpenMutex(SYNCHRONIZE, FALSE, CONSOLE_MUTEX_NAME);
    if (!hConsoleMutex) {
        std::cerr << "Failed to open console mutex. Make sure receiver is running first." << std::endl;
        logger.Cleanup();
        return 1;
    }
    
    if (argc < 3) {
        WaitForSingleObject(hConsoleMutex, INFINITE);
        logger.Error("Usage: sender.exe <filename> <sender_id>");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hConsoleMutex});
        logger.Cleanup();
        return 1;
    }

    std::string filename = argv[1];
    int senderId = std::atoi(argv[2]);

    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Sender(senderId, "========== SENDER " + std::to_string(senderId) + " STARTED ==========");
    ReleaseMutex(hConsoleMutex);

    // Creating a file to write where
    HANDLE hFile = CreateFile(
        filename.c_str(), 
        GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    // Error handling
    if(hFile == INVALID_HANDLE_VALUE){
        WaitForSingleObject(hConsoleMutex, INFINITE);
        PrintError("CreateFile error - make sure receiver created the file first");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hConsoleMutex});
        logger.Cleanup();
        return 1;
    }

    
    // A synchronization objects
    HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);
    HANDLE hEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, EMPTY_SEM_NAME);
    HANDLE hFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, FULL_SEM_NAME);

    if(!hMutex || !hEmpty || !hFull){
        WaitForSingleObject(hConsoleMutex, INFINITE);
        PrintError("Failed to open sync objects");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hFile, hMutex, hEmpty, hFull, hConsoleMutex});
        logger.Cleanup();
        return 1;
    }

    // Signal ready to receiver
    std::string eventName = std::string(READY_EVENT_PREFIX) + std::to_string(senderId);
    HANDLE hReadyEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, eventName.c_str());

    if(hReadyEvent){
        SetEvent(hReadyEvent);
        CloseHandle(hReadyEvent);
        WaitForSingleObject(hConsoleMutex, INFINITE);
        logger.Sender(senderId, "Signaled ready to receiver");
        ReleaseMutex(hConsoleMutex);
    }
    else{
        WaitForSingleObject(hConsoleMutex, INFINITE);
        PrintError("Failed to open ready event");
        ReleaseMutex(hConsoleMutex);
        CloseHandles({hFile, hMutex, hEmpty, hFull, hConsoleMutex});
        logger.Cleanup();
        return 1;
    }

    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Success("Sender " + std::to_string(senderId) + " is ready");
    ReleaseMutex(hConsoleMutex);

    // Main comman loop
    bool once = true;
    while(true){
        WaitForSingleObject(hConsoleMutex, INFINITE);
        if(once){
            DisplaySenderMenu(senderId);
            once = false;
        }
        logger.Sender(senderId, "Sender> ", false);
        
        std::string command;
        std::cin >> command;
        
        if (command == "/send") {
            SendMessage(hFile, hMutex, hEmpty, hFull, senderId, hConsoleMutex);
        }
        else if (command == "/exit") {
            logger.Sender(senderId, "Shutting down...");
            break;
        }
        else if (command == "/help") {
            DisplaySenderMenu(senderId);
        }
        else {
            logger.Warning("Unknown command. Use /help to see available");
        }
        ReleaseMutex(hConsoleMutex);
    }

    // Closing handles and finishing
    CloseHandles({hFile, hMutex, hEmpty, hFull});
    
    WaitForSingleObject(hConsoleMutex, INFINITE);
    logger.Sender(senderId, "Terminated.");
    ReleaseMutex(hConsoleMutex);
    CloseHandles({hConsoleMutex});
    logger.Cleanup();

    return 0;
}