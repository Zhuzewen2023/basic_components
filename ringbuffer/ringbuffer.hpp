#ifndef __RINGBUFFER_HPP__
#define __RINGBUFFER_HPP__

#include <atomic>
#include <type_traits>

template <typename T, std::size_t Capacity>
class RingBuffer
{
public:
	static_assert(Capacity && !(Capacity& (Capacity - 1)), "Capacity must be power of 2");
	RingBuffer() : read_(0), write_(0) {}
	~RingBuffer() {
		/*只要完成了线程切换，relaxed也可以拿到最新值*/
		std::size_t r = read_.load(std::memory_order_relaxed);
		const std::size_t w = write_.load(std::memory_order_relaxed);
		while (r != w) {
			reinterpret_cast<T*>(&buffer_[r])->~T();
			r = (r + 1) & (Capacity - 1);
		}
	}
	
	template<typename U>
	bool push(U&& value) {
		const std::size_t w = write_.load(std::memory_order_relaxed); /*在SPSC的情况下，relaxed也可以保证可见性*/
		const std::size_t next_w = (w + 1) & (Capacity - 1);
		if (next_w == read_.load(std::memory_order_acquire)) {
			return false;
		}
		new (&buffer_[w]) T(std::forward<U>(value));
		write_.store(next_w, std::memory_order_release);
		return true;
	}

	bool pop(T& value) {
		const std::size_t r = read_.load(std::memory_order_relaxed);
		if (r == write_.load(std::memory_order_acquire)) {
			return false;
		}
		value = std::move(*reinterpret_cast<T*>(&buffer_[r]));
		reinterpret_cast<T*>(&buffer_[r])->~T();
		read_.store((r + 1) & (Capacity - 1), std::memory_order_release);
		return true;
	}

	std::size_t size() const {
		const std::size_t r = read_.load(std::memory_order_acquire);
		const std::size_t w = write_.load(std::memory_order_acquire);
		return (w >= r) ? (w - r) : (Capacity - r + w);
	}

private:
	/*read和write不应该在一个cacheline里面，否则缓存失效的话read_和write_同时失效，造成伪共享的问题*/
	alignas(64) std::atomic<std::size_t> read_; /*alignas设置64个字节对齐，防止在一个cacheline*/
	alignas(64) std::atomic<std::size_t> write_;
	alignas(64) std::aligned_storage_t<sizeof(T), alignof(T)> buffer_[Capacity]; /*通过placement new兼容支持POD和非POD类型*/
};

#endif
