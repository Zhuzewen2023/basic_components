#ifndef __TIMER_WITH_MULTIMAP_HPP__
#define __TIMER_WITH_MULTIMAP_HPP__

#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <map>
#include <memory>
#include <functional>
#include <chrono>

class TimerNode
{
public:
	friend class Timer;
	TimerNode(uint64_t timeout = 0, std::function<void()> callback = nullptr) : timeout_(timeout), callback_(std::move(callback)) {}
private:
	uint64_t timeout_;
	std::function<void()> callback_;
};

class Timer
{
public:
	using TimerNodePtr = TimerNode*; /*长时间拥有指针是有风险的*/

	static uint64_t get_current_time() {
		using namespace std::chrono;
		return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count(); /*使用单调递增时钟，从系统启动开始计算，不受系统时间修改影响*/
	}

	TimerNodePtr add_timeout(uint64_t diff, std::function<void()> cb) {
		if (0 == diff) { /*避免立刻超时的无效任务*/
			return nullptr;
		}
		auto node = std::make_unique<TimerNode>();
		node->timeout_ = get_current_time() + diff;
		node->callback_ = std::move(cb);
		if (timer_map_.empty() || node->timeout_ < timer_map_.rbegin()->first) {
			auto it = timer_map_.emplace(node->timeout_, std::move(node));
			return it->second.get();
		}
		else {
			auto it = timer_map_.emplace_hint(timer_map_.end(), node->timeout_, std::move(node));
			return it->second.get();
		}
		return nullptr;
	}

	void del_timeout(TimerNodePtr node) {
		if (!node) return;
		auto range = timer_map_.equal_range(node->timeout_); /*equal_range用于返回key相同的所有键值对*/
		for (auto iter = range.first; iter != range.second; ++iter) {
			if (iter->second.get() == node) {
				timer_map_.erase(iter);
				break;
			}
		}
	}

	int wait_time() {
		auto iter = timer_map_.begin();
		if (iter == timer_map_.end()) {
			return -1;
		}
		uint64_t diff = iter->first - get_current_time();
		return diff > 0 ? diff : 0;
	}

	void handle_timeout() {
		auto iter = timer_map_.begin();
		while (iter != timer_map_.end() && iter->first <= get_current_time()) {
			if (iter->second->callback_) {
				iter->second->callback_();
			}
			iter = timer_map_.erase(iter);
		}
	}

private:
	std::multimap<uint64_t, std::unique_ptr<TimerNode>> timer_map_;
};

#endif
