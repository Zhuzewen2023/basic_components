#include "message_buffer.hpp"
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>

// 测试1：初始化与基本属性
void test_initialization() {
    // 默认构造
    MessageBuffer buf1;
    assert(buf1.get_buffer_size() == 4096);
    assert(buf1.get_active_size() == 0);
    assert(buf1.get_free_size() == 4096);

    // 指定大小构造
    MessageBuffer buf2(1024);
    assert(buf2.get_buffer_size() == 1024);
    assert(buf2.get_active_size() == 0);
}

// 测试2：写入与读取操作
void test_write_read() {
    MessageBuffer buf;
    const uint8_t data[] = "Hello, MessageBuffer!";
    std::size_t data_len = sizeof(data) - 1; // 排除终止符

    // 写入数据
    buf.write(data, data_len);
    assert(buf.get_active_size() == data_len);
    assert(buf.get_free_size() == 4096 - data_len);

    // 读取数据
    uint8_t read_buf[1024] = { 0 };
    std::memcpy(read_buf, buf.get_read_pointer(), data_len);
    buf.read_completed(data_len);
    assert(buf.get_active_size() == 0); // 全部读取完成
    assert(std::memcmp(read_buf, data, data_len) == 0); // 数据一致
}

// 测试3：normalize（缓冲区整理）功能
void test_normalize() {
    MessageBuffer buf(1024);
    const uint8_t data[] = "Test normalize";
    std::size_t data_len = sizeof(data) - 1;

    // 写入数据后读取一部分，制造前端空洞
    buf.write(data, data_len);
    buf.read_completed(5); // 读取前5字节，rpos_=5
    assert(buf.get_active_size() == data_len - 5);
    assert(buf.get_read_pointer() == buf.get_base_pointer() + 5);

    // 执行整理
    buf.normalize();
    assert(buf.get_read_pos() == 0); // 读指针重置
    assert(buf.get_write_pos() == data_len - 5); // 写指针调整
    assert(buf.get_read_pointer() == buf.get_base_pointer()); // 读指针指向起始位置

    // 验证数据正确性（剩余数据应为"normalize"）
    const uint8_t expected[] = "normalize";
    assert(std::memcmp(buf.get_read_pointer(), expected, sizeof(expected) - 1) == 0);
}

// 测试4：自动扩容功能
void test_ensure_free_space() {
    MessageBuffer buf(100); // 初始大小100
    assert(buf.get_buffer_size() == 100);

    // 写入80字节，剩余空间20
    uint8_t data[80] = { 0 };
    buf.write(data, 80);
    assert(buf.get_free_size() == 20);

    // 需要30字节空间，当前不足，触发扩容
    buf.ensure_free_space(30);
    // 扩容策略：max(100+30, 100*1.5)=150，因此新大小应为150
    assert(buf.get_buffer_size() >= 130); // 至少能容纳原有80+新增30

    // 验证原有数据未丢失
    assert(std::memcmp(buf.get_read_pointer(), data, 80) == 0);
}

// 测试5：recv函数（网络数据接收）
void test_recv() {
    // 创建一对本地套接字用于测试
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    int fd_read = fds[0];
    int fd_write = fds[1];

    // 发送端写入数据
    const char* send_data = "Test recv function with MessageBuffer";
    ssize_t send_len = write(fd_write, send_data, strlen(send_data));
    assert(send_len == static_cast<ssize_t>(strlen(send_data)));

    // 接收端用MessageBuffer接收
    MessageBuffer buf;
    int err = 0;
    ssize_t recv_len = buf.recv(fd_read, &err);
    assert(err == 0);
    assert(recv_len == send_len);
    assert(buf.get_active_size() == send_len);
    assert(std::memcmp(buf.get_read_pointer(), send_data, send_len) == 0);

    // 关闭套接字
    close(fd_read);
    close(fd_write);
}

// 测试6：移动语义
void test_move_semantics() {
    MessageBuffer buf1(1024);
    const uint8_t data[] = "Move test data";
    buf1.write(data, sizeof(data) - 1);
    std::size_t active_size = buf1.get_active_size();

    // 移动构造
    MessageBuffer buf2(std::move(buf1));
    assert(buf1.get_active_size() == 0); // 原对象被清空
    assert(buf2.get_active_size() == active_size);
    assert(std::memcmp(buf2.get_read_pointer(), data, active_size) == 0);

    // 移动赋值
    MessageBuffer buf3;
    buf3 = std::move(buf2);
    assert(buf2.get_active_size() == 0); // 原对象被清空
    assert(buf3.get_active_size() == active_size);
    assert(std::memcmp(buf3.get_read_pointer(), data, active_size) == 0);
}

int main() {
    test_initialization();
    test_write_read();
    test_normalize();
    test_ensure_free_space();
    test_recv();
    test_move_semantics();

    printf("All tests passed!\n");
    return 0;
}
