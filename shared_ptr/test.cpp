#include "shared_ptr.hpp"
#include <iostream>
#include <thread>
#include <cassert>
#include <vector>

// 测试基础功能
void test_basic_functionality() {
    std::cout << "测试基础功能..." << std::endl;
    
    // 测试默认构造
    shared_ptr<int> sp1;
    assert(sp1.get() == nullptr);
    assert(sp1.use_count() == 0);
    
    // 测试带指针的构造
    shared_ptr<int> sp2(new int(42));
    assert(sp2.get() != nullptr);
    assert(*sp2 == 42);
    assert(sp2.use_count() == 1);
    
    // 测试拷贝构造
    shared_ptr<int> sp3 = sp2;
    assert(sp3.get() == sp2.get());
    assert(*sp3 == 42);
    assert(sp2.use_count() == 2);
    assert(sp3.use_count() == 2);
    
    // 测试拷贝赋值
    shared_ptr<int> sp4;
    sp4 = sp3;
    assert(sp4.get() == sp2.get());
    assert(sp2.use_count() == 3);
    
    // 测试reset
    sp4.reset();
    assert(sp4.get() == nullptr);
    assert(sp2.use_count() == 2);
    
    // 测试operator*和operator->
    *sp2 = 100;
    assert(*sp3 == 100);
    
    struct TestStruct { int x; };
    shared_ptr<TestStruct> sp5(new TestStruct{5});
    assert(sp5->x == 5);
    sp5->x = 10;
    assert(sp5->x == 10);
    
    std::cout << "基础功能测试通过！\n" << std::endl;
}

// 测试移动语义
void test_move_semantics() {
    std::cout << "测试移动语义..." << std::endl;
    
    shared_ptr<int> sp1(new int(42));
    assert(sp1.use_count() == 1);
    int* ptr = sp1.get();
    
    // 测试移动构造
    shared_ptr<int> sp2(std::move(sp1));
    assert(sp1.get() == nullptr);  // 原指针应被置空
    assert(sp2.get() == ptr);      // 新指针应指向原内存
    assert(sp2.use_count() == 1);
    
    // 测试移动赋值
    shared_ptr<int> sp3(new int(100));
    int* ptr3 = sp3.get();
    sp3 = std::move(sp2);
    assert(sp2.get() == nullptr);  // 被移动的指针应被置空
    assert(sp3.get() == ptr);      // 目标指针应指向新内存
    assert(*sp3 == 42);
    assert(sp3.use_count() == 1);
    
    // 测试自移动（应安全处理）
    sp3 = std::move(sp3);
    assert(sp3.get() == ptr);
    assert(sp3.use_count() == 1);
    
    std::cout << "移动语义测试通过！\n" << std::endl;
}

// 测试多线程安全性
void test_thread_safety() {
    std::cout << "测试多线程安全性..." << std::endl;
    
    const int THREAD_COUNT = 10;
    const int OPERATIONS_PER_THREAD = 100000;
    
    shared_ptr<std::atomic<int>> sp(new std::atomic<int>(0));
    std::vector<std::thread> threads;
    
    // 多个线程同时进行拷贝、赋值和修改操作
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                // 拷贝shared_ptr
                shared_ptr<std::atomic<int>> local_sp = sp;
                
                // 修改共享数据
                (*local_sp)++;
                
                // 移动操作
                shared_ptr<std::atomic<int>> temp = std::move(local_sp);
                sp = std::move(temp);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证结果：每个线程执行了OPERATIONS_PER_THREAD次++
    assert(*sp == THREAD_COUNT * OPERATIONS_PER_THREAD);
    // 最终引用计数应为1（只有sp持有）
    assert(sp.use_count() == 1);
    
    std::cout << "多线程安全性测试通过！\n" << std::endl;
}

// 测试资源释放
void test_resource_deallocation() {
    std::cout << "测试资源释放..." << std::endl;
    
    int* raw_ptr = new int(42);
    {
        shared_ptr<int> sp1(raw_ptr);
        {
            shared_ptr<int> sp2 = sp1;
            assert(sp1.use_count() == 2);
            assert(sp2.use_count() == 2);
        }
        assert(sp1.use_count() == 1);
    }
    // 此时raw_ptr应已被释放，无法安全访问
    
    // // 使用标志测试对象是否被释放
    // bool* destroyed = new bool(false);
    // int* test_ptr = new int(0);
    
    // {
    //     shared_ptr<int> sp(test_ptr);
    //     shared_ptr<bool> destroy_flag(destroyed);
        
    //     // 当sp离开作用域时，test_ptr应被释放
    //     // 当destroy_flag离开作用域时，destroyed应被设为true
    //     *destroy_flag = true;
    // }
    
    // assert(*destroyed == true);  // 验证标志已被正确设置
    
    std::cout << "资源释放测试通过！\n" << std::endl;
}

// 测试空指针和异常情况
void test_edge_cases() {
    std::cout << "测试边界情况..." << std::endl;
    
    // 测试空指针的各种操作
    shared_ptr<int> sp1(nullptr);
    assert(sp1.get() == nullptr);
    assert(sp1.use_count() == 0);
    
    shared_ptr<int> sp2(new int(42));
    sp2.reset(nullptr);
    assert(sp2.get() == nullptr);
    assert(sp2.use_count() == 0);
    
    // 测试自赋值
    sp2.reset(new int(100));
    sp2 = sp2;  // 拷贝自赋值
    assert(sp2.use_count() == 1);
    assert(*sp2 == 100);
    
    sp2 = std::move(sp2);  // 移动自赋值
    assert(sp2.use_count() == 1);
    assert(*sp2 == 100);
    
    // 测试不同类型的指针
    shared_ptr<const int> sp3(new int(5));
    assert(*sp3 == 5);
    // *sp3 = 6;  // 应编译错误，const对象不能修改
    
    std::cout << "边界情况测试通过！\n" << std::endl;
}

int main() {
    test_basic_functionality();
    test_move_semantics();
    test_thread_safety();
    test_resource_deallocation();
    test_edge_cases();
    
    std::cout << "所有测试均通过！" << std::endl;
    return 0;
}
