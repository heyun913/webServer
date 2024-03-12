#include "stream.h"

namespace sylar {

// 读固定长度的数据，读取到内存
int Stream::readFixSize(void* buffer, size_t length) {
    // 偏移量
    size_t offset = 0;
    // 读取长度
    int64_t left = length;
    while(left > 0) {
        // len表示已经读取的数据大小
        int64_t len = read((char*)buffer + offset, left);
        // 异常
        if(len <= 0) {
            return len;
        }
        // 更新偏移量
        offset += len;
        // 更新剩余读取长度
        left -= len;
    }
    // 返回读取到内存中的数据长度
    return length;
}

// 读固定长度的数据，读取到ByteArray 因为ByteArray对象内部有一个位置指针，所以不需要手动更新偏移量
int Stream::readFixSize(ByteArray::ptr ba, size_t length) {
    int64_t left = length;
    while(left > 0) {
        int64_t len = read(ba, left);
        if(len <= 0) {
            return len;
        }
        left -= len;
    }
    return length;
}

// 写固定长度的数据，从内存写
int Stream::writeFixSize(const void* buffer, size_t length) {
    size_t offset = 0;
    int64_t left = length;
    while(left > 0) {
        int64_t len = write((const char*)buffer + offset, left);
        if(len <= 0) {
            return len;
        }
        offset += len;
        left -= len;
    }
    return length;

}

// 写固定长度的数据，从ByteArray写
int Stream::writeFixSize(ByteArray::ptr ba, size_t length) {
    int64_t left = length;
    while(left > 0) {
        int64_t len = write(ba, left);
        if(len <= 0) {
            return len;
        }
        left -= len;
    }
    return length;
}

}
