#pragma once

#include <windows.h>
#include <iostream>
#include <string>

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
const char* MUTEX_NAME = "Lab4_Mutex";
const char* EMPTY_SEM_NAME = "Lab4_EmptySem";
const char* FULL_SEM_NAME = "Lab4_FullSem";
const char* READY_EVENT_PREFIX = "Lab4_Ready_";


// Inline print error method
inline void PrintError(const std::string& msg){
    DWORD err = GetLastError();
    std::cout << msg << " Error code: " << std::to_string(err) << std::endl;
}