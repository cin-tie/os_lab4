#pragma once

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <initializer_list>
#include <cstring>
#include "logger.h"

const int MAX_MSG_SIZE = 20;

// Structs

struct Message {
    char text[MAX_MSG_SIZE];
};

struct FileHeader {
    int capacity;
    int head;
    int tail;
    int count;
};

// Synchronizaion objects names
const char* MUTEX_NAME = "Global\\Lab4_Mutex";
const char* EMPTY_SEM_NAME = "Global\\Lab4_EmptySem";
const char* FULL_SEM_NAME = "Global\\Lab4_FullSem";
const char* READY_EVENT_PREFIX = "Global\\Lab4_Ready_";
const char* CONSOLE_MUTEX_NAME = "Global\\Lab4_ConsoleMutex";


// Inline print error method
inline void PrintError(const std::string& msg){
    DWORD err = GetLastError();
    std::cout << msg << " Error code: " << std::to_string(err) << std::endl;
}

// Close all handles (various implementations)
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