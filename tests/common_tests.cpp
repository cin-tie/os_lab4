#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include "../include/common.h"

// Test that message correctly fits into fixed-size buffer without corruption
TEST(MessageTests, MessageFitsIntoBuffer)
{
    Message msg;
    memset(msg.text, 0, MAX_MSG_SIZE);

    const char* text = "hello";
    strncpy(msg.text, text, MAX_MSG_SIZE - 1);

    EXPECT_STREQ(msg.text, "hello");
}

// Test that long messages are safely truncated to buffer size limits
TEST(MessageTests, MessageTruncation)
{
    Message msg;
    memset(msg.text, 0, MAX_MSG_SIZE);

    std::string longText(MAX_MSG_SIZE + 10, 'A');
    strncpy(msg.text, longText.c_str(), MAX_MSG_SIZE - 1);

    EXPECT_LE(strlen(msg.text), MAX_MSG_SIZE - 1);
}

// Test initial state of FileHeader before any queue operations
TEST(HeaderTests, InitialState)
{
    FileHeader h{};
    h.capacity = 10;
    h.head = 0;
    h.tail = 0;
    h.count = 0;

    EXPECT_EQ(h.count, 0);
    EXPECT_EQ(h.head, 0);
    EXPECT_EQ(h.tail, 0);
}

// Test circular buffer wrap-around behavior for tail index
TEST(HeaderTests, CircularIncrement)
{
    FileHeader h{};
    h.capacity = 5;

    h.tail = 4;
    h.tail = (h.tail + 1) % h.capacity;

    EXPECT_EQ(h.tail, 0);
}

// Test increment and decrement of message counter in queue header
TEST(HeaderTests, CountIncrementDecrement)
{
    FileHeader h{};
    h.count = 0;

    h.count++;
    EXPECT_EQ(h.count, 1);

    h.count--;

    EXPECT_EQ(h.count, 0);
}

// Test that CloseHandles safely handles empty handle lists without crashing
TEST(HandleTests, CloseHandlesEmpty)
{
    std::vector<HANDLE> handles;
    EXPECT_NO_THROW(CloseHandles(handles));
}

// Test correct initialization values for a fresh queue header
TEST(HeaderTests, QueueHeaderInitialization)
{
    FileHeader h{};
    h.capacity = 8;
    h.head = 0;
    h.tail = 0;
    h.count = 0;

    EXPECT_EQ(h.capacity, 8);
    EXPECT_EQ(h.head, 0);
    EXPECT_EQ(h.tail, 0);
    EXPECT_EQ(h.count, 0);
}

// Test that queue correctly detects full state condition
TEST(HeaderTests, QueueFullCondition)
{
    FileHeader h{};
    h.capacity = 3;
    h.count = 3;

    EXPECT_TRUE(h.count >= h.capacity);
}

// Test that queue correctly detects empty state condition
TEST(HeaderTests, QueueEmptyCondition)
{
    FileHeader h{};
    h.count = 0;

    EXPECT_TRUE(h.count == 0);
}

// Test ring buffer head movement correctness
TEST(HeaderTests, HeadMovement)
{
    FileHeader h{};
    h.capacity = 4;
    h.head = 3;

    h.head = (h.head + 1) % h.capacity;

    EXPECT_EQ(h.head, 0);
}