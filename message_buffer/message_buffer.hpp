#ifndef __MESSAGE_BUFFER_HPP__
#define __MESSAGE_BUFFER_HPP__

#include <cstdint>
#include <vector>
#include <cstring>
#include <sys/uio.h>
#include <errno.h>

class MessageBuffer
{
public:
	MessageBuffer() : rpos_(0), wpos_(0)
	{
		buffer_.resize(4096);
	}

	explicit MessageBuffer(std::size_t size) : rpos_(0), wpos_(0)
	{
		if (size == 0) size = 4096;
		buffer_.resize(size);
	}

	MessageBuffer(const MessageBuffer&) = delete;
	MessageBuffer& operator=(const MessageBuffer&) = delete;

	MessageBuffer(MessageBuffer&& other) noexcept
		: buffer_(std::move(other.buffer_)), rpos_(other.rpos_), wpos_(other.wpos_)
	{
		other.rpos_ = 0;
		other.wpos_ = 0;
	}

	MessageBuffer& operator=(MessageBuffer&& other) noexcept
	{
		if (this != &other) {
			buffer_ = std::move(other.buffer_);
			rpos_ = other.rpos_;
			wpos_ = other.wpos_;
			other.rpos_ = 0;
			other.wpos_ = 0;
		}
		return *this;
	}

	uint8_t* get_base_pointer() {
		return buffer_.data();
	}

	uint8_t* get_read_pointer() {
		return buffer_.data() + rpos_;
	}

	uint8_t* get_write_pointer() {
		return buffer_.data() + wpos_;
	}

	void read_completed(std::size_t size) {
		if (size > get_active_size()) return;
		rpos_ += size;
	}

	void write_completed(std::size_t size) {
		if (wpos_ + size > buffer_.size()) return;
		wpos_ += size;
	}

	std::size_t get_active_size() const {
		return wpos_ - rpos_;
	}

	std::size_t get_free_size() const {
		return buffer_.size() - wpos_;
	}

	std::size_t get_buffer_size() const {
		return buffer_.size();
	}

	void normalize() {
		if (rpos_ == 0 || get_active_size() == 0) {
			return;
		}
		if (rpos_ > wpos_) {
			rpos_ = wpos_ = 0;
			return;
		}
		if (rpos_ > 0) {
			std::memmove(buffer_.data(), buffer_.data() + rpos_, get_active_size());
			wpos_ -= rpos_;
			rpos_ = 0;
		}
	}

	void ensure_free_space(std::size_t size) {
		if (size == 0) return;
		if (get_free_size() >= size) {
			return;
		}
		normalize();

		if (get_free_size() < size) {
			std::size_t new_size = std::max(buffer_.size() + size, buffer_.size() * 3 / 2);
			buffer_.resize(new_size);
		}

	}

	void write(const uint8_t* data, std::size_t size) {
		if (NULL == data || 0 == size) return;
		if (size > 0) {
			ensure_free_space(size);
			std::memcpy(get_write_pointer(), data, size);
			write_completed(size);
		}
	}

	std::size_t get_read_pos() const {
		return rpos_;
	}

	std::size_t get_write_pos() const {
		return wpos_;
	}

	int recv(int fd, int* err) {
		if (nullptr == err) return -1;
		*err = 0;
		char extra[65535];
		struct iovec iov[2];
		iov[0].iov_base = get_write_pointer();
		iov[0].iov_len = get_free_size();
		iov[1].iov_base = extra;
		iov[1].iov_len = sizeof(extra);
		ssize_t n = readv(fd, iov, 2);
		if (n < 0) {
			*err = errno;
			return -1;
		}
		if (n == 0) {
			*err = 0;
			return 0;
	}
		if (static_cast<std::size_t>(n) <= get_free_size()) {
			write_completed(n);
			
		}
		else {
			std::size_t extra_size = n - get_free_size();
			write_completed(get_free_size());
			write(reinterpret_cast<uint8_t*>(extra), extra_size);
		}
		return n;

	}

private:
	std::vector<uint8_t> buffer_;
	std::size_t rpos_;
	std::size_t wpos_;
};

#endif

