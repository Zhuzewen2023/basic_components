#ifndef __SHARED_PTR_HPP__
#define __SHARED_PTR_HPP__

#include <atomic>

template <typename T>
class shared_ptr
{
public:
    shared_ptr() : ptr_(nullptr), ref_count_(nullptr) {}
    explicit shared_ptr(T* ptr) : ptr_(ptr), ref_count_(ptr ? new std::atomic<std::size_t>(1) : nullptr) 
    {

    }

    ~shared_ptr()
    {
        release();
    }

    shared_ptr(const shared_ptr<T>& other) : ptr_(other.ptr_), ref_count_(other.ref_count_)
    {
        if (ref_count_)
        {
            ref_count_->fetch_add(1, std::memory_order_relaxed); /*使用std::memory_order_relaxed可以避免不必要的内存屏障*/
        }
    }

    shared_ptr<T>& operator=(const shared_ptr<T>& other)/*需要自赋值检查 */
    {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            if (ref_count_) {
                ref_count_->fetch_add(1, std::memory_order_relaxed); /*使用std::memory_order_relaxed可以避免不必要的内存屏障*/
            }
        }
        return *this;
    }

    shared_ptr(shared_ptr<T>&& other) noexcept /*告诉编译器不要做异常处理，如果涉及到vector，不用noexcept不会使用移动构造*/
    {
        ptr_ = other.ptr_;
        ref_count_ = other.ref_count_;
        other.ptr_ = nullptr;
        other.ref_count_ = nullptr;
    }

    shared_ptr<T>& operator=(shared_ptr<T>&& other) noexcept/*需要自赋值检查 */
    {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            other.ptr_ = nullptr;
            other.ref_count_ = nullptr;
        }
        return *this;
    }

    T& operator*() const
    {
        return *ptr_;
    }

    T* operator->() const
    {
        return ptr_;
    }

    std::size_t use_count() const
    {
        return ref_count_ ? ref_count_->load(std::memory_order_acquire) : 0;
    }

    T* get() const
    {
        return ptr_;
    }

    void reset(T* ptr = nullptr)
    {
        if (ptr_ != ptr) {
            release();
            ptr_ = ptr;
            ref_count_ = ptr ? new std::atomic<std::size_t>(1) : nullptr;
        }
    }

private:
    void release()
    {
        if (ref_count_ && ref_count_->fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete ptr_;
            delete ref_count_;
        }
    }
    T* ptr_;
    std::atomic<std::size_t>* ref_count_;
};


#endif
