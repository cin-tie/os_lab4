#include <windows.h>
#include <iostream>
#include <string>
#include "../include/common.h"

// Display sender menu
void DisplaySenderMenu(int id) {
    std::cout << "\n========== SENDER " << id << " MENU ==========" << std::endl;
    std::cout << "/send   - Send message to receiver" << std::endl;
    std::cout << "/exit   - Exit sender process" << std::endl;
    std::cout << "/help   - Show this menu" << std::endl;
    std::cout << "=====================================" << std::endl;
}

// Send message to queue
bool SendMessage(HANDLE hFile, HANDLE hMutex, HANDLE hEmpty, HANDLE hFull, int senderId){
    std::cout << "Enter message (max " << MAX_MSG_SIZE - 1 << " chars): ";
    std::string message;
    std::cin.ignore(); // Clear input buffer
    std::getline(std::cin, message);

    // Validate message
    if(message.empty()){
        std::cout << "Error: Message cannot be empty!" << std::endl;
        return false;
    }

    if (message.length() >= MAX_MSG_SIZE) {
        std::cout << "Error: Message too long! Maximum " << MAX_MSG_SIZE - 1 << " characters." << std::endl;
        return false;
    }

    // Wait for empty slot in queue
    std::cout << "Waiting for free slot in queue..." << std::endl;
    DWORD waitResult = WaitForSingleObject(hEmpty, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
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
        std::cout << "Error: Queue is full!" << std::endl;
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
    
    return true;
}

int main(int argc, char* argv[]){
    if (argc < 3) {
        std::cout << "Usage: sender.exe <filename> <sender_id>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    int senderId = std::atoi(argv[2]);

    std::cout << "========== SENDER " << senderId << " STARTED ==========" << std::endl;

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

    
    // A synchronization objects
    HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);
    HANDLE hEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, EMPTY_SEM_NAME);
    HANDLE hFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, FULL_SEM_NAME);

    if(!hMutex || !hEmpty || !hFull){
        PrintError("Failed to open sync objects");
        CloseHandles({hFile, hMutex, hEmpty, hFull});
        return 1;
    }

    // Signal ready to receiver
    std::string eventName = std::string(READY_EVENT_PREFIX) + std::to_string(senderId);
    HANDLE hReadyEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, eventName.c_str());

    if(hReadyEvent){
        SetEvent(hReadyEvent);
        CloseHandle(hReadyEvent);
        std::cout << "Sender " << senderId << " signaled ready" << std::endl;
    }
    else{
        PrintError("Failed to open ready event");
        CloseHandles({hFile, hMutex, hEmpty, hFull});
        return 1;
    }

    std::cout << "Sender " << senderId << " is ready" << std::endl;
    DisplaySenderMenu(senderId);

    // Main comman loop
    bool running = true;
    while(running){
        std::string command;
        std::cout << "\nSender " << senderId << "> ";
        std::cin >> command;

        if (command == "/send") {
            SendMessage(hFile, hMutex, hEmpty, hFull, senderId);
        }
        else 
        if (command == "/exit") {
            std::cout << "Sender " << senderId << " shutting down..." << std::endl;
            running = false;
        }
        else 
        if (command == "/help") {
            DisplaySenderMenu(senderId);
        }
        else {
            std::cout << "Unknown command\nUse /help to see available" << std::endl;
        }
    }

    // Closing handles and finishing
    CloseHandles({hFile, hMutex, hEmpty, hFull});
    std::cout << "Sender " << senderId << " terminated." << std::endl;

    return 0;
}