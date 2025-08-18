#include "ringbuffer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cassert>
#include <chrono>

// 测试POD类型（int）
void test_pod_type() {
    std::cout << "Testing POD type (int)..." << std::endl;
    RingBuffer<int, 8> rb;

    // 测试push和pop
    assert(rb.push(10) == true);
    assert(rb.push(20) == true);
    assert(rb.size() == 2);

    int val;
    assert(rb.pop(val) == true);
    assert(val == 10);
    assert(rb.pop(val) == true);
    assert(val == 20);
    assert(rb.size() == 0);

    // 测试缓冲区满的情况
    for (int i = 0; i < 7; ++i) {
        assert(rb.push(i) == true);
    }
    assert(rb.push(9) == false);  // 缓冲区已满
    assert(rb.size() == 7);

    std::cout << "POD type test passed.\n" << std::endl;
}

// 测试非POD类型（std::string）
void test_non_pod_type() {
    std::cout << "Testing non-POD type (std::string)..." << std::endl;
    RingBuffer<std::string, 4> rb;

    // 测试push和pop
    assert(rb.push(std::string("hello")) == true);
    assert(rb.push("world") == true);  // 测试隐式转换
    assert(rb.size() == 2);

    std::string val;
    assert(rb.pop(val) == true);
    assert(val == "hello");
    assert(rb.pop(val) == true);
    assert(val == "world");
    assert(rb.size() == 0);

    std::cout << "Non-POD type test passed.\n" << std::endl;
}

// 测试单生产者单消费者多线程场景
void test_spsc_multithread() {
    std::cout << "Testing SPSC multithread scenario..." << std::endl;
    const int DATA_SIZE = 100000;
    RingBuffer<int, 1024> rb;

    // 生产者线程：写入数据
    std::thread producer([&rb]() {
        for (int i = 0; i < DATA_SIZE; ++i) {
            // 循环直到成功写入
            while (!rb.push(i)) {
                std::this_thread::yield();  // 让出CPU
            }
        }
        });

    // 消费者线程：读取数据并验证
    std::thread consumer([&rb]() {
        int expected = 0;
        int val;
        while (expected < DATA_SIZE) {
            // 循环直到成功读取
            while (!rb.pop(val)) {
                std::this_thread::yield();  // 让出CPU
            }
            assert(val == expected);  // 验证数据顺序和正确性
            expected++;
        }
        });

    producer.join();
    consumer.join();
    assert(rb.size() == 0);  // 所有数据应被消费完毕

    std::cout << "SPSC multithread test passed.\n" << std::endl;
}

// 测试缓冲区边界情况
void test_edge_cases() {
    std::cout << "Testing edge cases..." << std::endl;
    RingBuffer<int, 4> rb;  // 容量为2（最小的2的幂）

    // 测试写满后读取再写入
    assert(rb.push(1) == true);
    assert(rb.push(2) == true);
    assert(rb.push(3) == true);
    assert(rb.push(4) == false);// 已满

    int val;
    assert(rb.pop(val) == true);
    assert(val == 1);
    assert(rb.push(4) == true);  // 现在有空间

    assert(rb.pop(val) == true);
    assert(val == 2);
    assert(rb.pop(val) == true);
    assert(val == 3);
    assert(rb.pop(val) == true);
    assert(val == 4);
    assert(rb.pop(val) == false);// 已空

    // 测试环形绕回
    assert(rb.push(10) == true);
    assert(rb.push(20) == true);
    assert(rb.push(30) == true);
    assert(rb.push(40) == false);
    assert(rb.pop(val) == true);
    assert(rb.pop(val) == true);
    assert(rb.push(40) == true); 
    assert(rb.push(50) == true);// 写入位置绕回0
    assert(rb.size() == 3);

    std::cout << "Edge cases test passed.\n" << std::endl;
}

int main() {
    test_pod_type();
    test_non_pod_type();
    test_edge_cases();
    test_spsc_multithread();

    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
