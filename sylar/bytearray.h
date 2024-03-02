#ifndef __SYLAR_BYTEARRAY_H__
#define __SYLAR_BYTEARRAY_H__

#include <memory>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<vector>

namespace sylar {


class ByteArray {
public:
    typedef std::shared_ptr<ByteArray> ptr;
    
    // 存储数据的链表结构
    struct Node {
        Node(size_t s);
        Node();
        ~Node();

        char* ptr;
        Node* next;
        size_t size;
    };

    // 构造函数，默认内存容量为4M
    ByteArray(size_t base_size = 4096);

    // 析构函数
    ~ByteArray();


    // 固定长度写入
    void writeFint8(int8_t value); 
    void writeFuint8(uint8_t value);
    void writeFint16(int16_t value);
    void writeFuint16(uint16_t value);
    void writeFint32(int32_t value);
    void writeFuint32(uint32_t value);
    void writeFint64(int64_t value);
    void writeFuint64(uint64_t value);

    // 不固定长度写入
    void writeInt32(int32_t value);
    void writeUint32(uint32_t value);
    void writeInt64(int64_t value);
    void writeUint64(uint64_t value);

    void writeFloat(float value);
    void writeDouble(double value);

    void writeStringF16(const std::string& value);
    void writeStringF32(const std::string& value);
    void writeStringF64(const std::string& value);
    // 压缩int长度
    void writeStringVint(const std::string& value);
    void writeStringWithoutLength(const std::string& value);

    // read
    int8_t readFint8();
    uint8_t readFuint8();
    int16_t readFint16();
    uint16_t readFuint16();
    int32_t readFint32();
    uint32_t readFuint32();
    int64_t readFint64();
    uint64_t readFuint64();

    int32_t readInt32();
    uint32_t readUint32();
    int64_t readInt64();
    uint64_t readUint64();

    float readFloat();
    double readDouble();

    std::string readStringF16();
    std::string readStringF32();
    std::string readStringF64();
    std::string readStringVint();

    // 清空ByteArray
    void clear();

    // 写入数据
    void write(const void* buf, size_t size);

    // 读取数据
    void read(void* buf, size_t size);
    void read(void* buf, size_t size, size_t position) const;

    // 获取当前节点的操作位置
    size_t getPosition() const { return m_position; }
    
    // 设置当前节点的操作位置
    void setPosition(size_t v);

    // 把数据写入文件
    bool writeToFile(const std::string& name) const;
    // 从文件中读取数据
    bool readFromFile(const std::string& name);

    // 获得一个节点的基础大小
    size_t getBaseSize() const { return m_baseSize; }
    // 获取剩余可读数据大小
    size_t getReadSize() const { return m_size - m_position; }
    // 判断字节序
    bool isLittleEndian() const;
    // 设置字节序
    void setIsLittleEndian(bool val);
    // 将当前对象中可读的数据以字符串方式输出
    std::string toString() const;
    // 同上，转换成十六进制输出
    std::string toHexString() const;
    // 获取可读取数据的缓冲区
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;
    // 同上，但是读取完毕不改变操作位置
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;
    // 获取可写入数据的缓冲区
    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

    size_t getSize() const { return m_size; }

private:
    // 增加容量
    void addCapacity(size_t size);
    // 获得容量
    size_t getCapacity() const { return m_capacity - m_position; }
private:
    // 内存块的大小
    size_t m_baseSize;
    // 当前操作的位置
    size_t m_position;
    // 总容量
    size_t m_capacity;
    // 当前数据的大小
    size_t m_size;
    // 字节序，默认大端
    int8_t m_endian;
    // 当一个内存块指针
    Node* m_root;
    // 当前操作的内存块指针
    Node* m_cur;
};


}



#endif