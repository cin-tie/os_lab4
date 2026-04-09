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

    std::cout << "Sender " << senderId << " is ready!" << std::endl;
    DisplaySenderMenu(senderId);
}