#include <iostream>
#include <sys/epoll.h>

#include "timer_with_multimap.hpp"

int main()
{
	int epfd = epoll_create1(0);
	if (epfd == -1) {
		std::cerr << "epoll_create error: " << errno << std::endl;
		return -1;
	}
	struct epoll_event evs[512];
	Timer timer;

	int i, j, k = 0;
	timer.add_timeout(1000, [&]() {
		std::cout << "Timeout 1 seconds: " << i++ << std::endl;
		});

	timer.add_timeout(2000, [&]() {
		std::cout << "Timeout 2 seconds: " << j++ << std::endl;
		});

	auto timer_ptr = timer.add_timeout(3000, [&]() {
		std::cout << "Timeout 3 seconds: " << k++ << std::endl;
		});

	timer.del_timeout(timer_ptr);

	while (true) {
		int n = epoll_wait(epfd, evs, 512, timer.wait_time()); /*-1 一直阻塞*/
		if (-1 == n) {
			if (EINTR == errno) {
				continue;
			}
			std::cerr << "epoll_wait error: " << errno << std::endl;
			break;
		}
		timer.handle_timeout();

	}
	return 0;
}