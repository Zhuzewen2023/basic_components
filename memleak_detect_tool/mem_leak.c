#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <link.h>

#if 0

void* my_malloc(size_t size, const char* filename, const char* funcname, int line)
{
	const char* dir = "./mem_block";
	if (access(dir, F_OK) == -1) {
		if (mkdir(dir, 0755) == -1) {
			perror("mkdir failed");
			return NULL;
		}
	}

	void* ptr = malloc(size);
	if (!ptr) {
		perror("malloc failed");
		return NULL;
	}

	char buff[128] = { 0 };
	snprintf(buff, sizeof(buff), "./mem_block/%p.mem", ptr);
	FILE* fp = fopen(buff, "w");
	if (!fp) {
		free(ptr);
		return NULL;
	}
	fprintf(fp, "[+][%s:%s:%d] %p: %ld my_malloc\n", filename, funcname, line, ptr, size);
	fflush(fp); /*强制刷新缓冲区，确保落盘*/
	fclose(fp);
	return ptr;
}

void my_free(void* ptr, const char* filename, const char* funcname, int line)
{
	char buff[128] = { 0 };
	snprintf(buff, sizeof(buff), "./mem_block/%p.mem", ptr);
	if (unlink(buff) < 0) {
		printf("double free: %p\n", ptr);
		return;
	}
	free(ptr);
	//printf("[-][%s:%s:%d] %p my_free\n", filename, funcname, line, ptr);
}

#define malloc(size) my_malloc(size, __FILE__, __func__, __LINE__)
#define free(ptr) my_free(ptr, __FILE__, __func__, __LINE__)
#else

/*hook*/
typedef void* (*malloc_t)(size_t size);
typedef void (*free_t)(void* ptr);

malloc_t malloc_f = NULL;
free_t free_f = NULL;

int enable_malloc = 1;
int enable_free = 1;

void* translate_address(void* addr)
{
	Dl_info info;
	struct link_map* link;
	/*
	* dladdr1 函数：用于解析一个内存地址对应的动态链接信息，参数说明：
	addr：待解析的运行时虚拟地址（如 caller）。
	info：输出参数，保存地址所属的文件名、符号名等信息。
	link：输出参数，通过 RTLD_DL_LINKMAP 标志获取 struct link_map* 结构体（动态库加载信息）。
	RTLD_DL_LINKMAP：标志位，要求 dladdr1 返回 link_map 结构体，用于获取加载基地址。
	*/
	if (dladdr1(addr, &info, (void*)&link, RTLD_DL_LINKMAP) != 0 && link) {
		printf("Address %p belongs to :%s\n", addr, info.dli_fname); /*打印所属文件*/
		return (void*)(addr - link->l_addr);
	}
	/*
	* struct link_map 结构体：动态链接器维护的动态库 / 可执行文件的加载信息，
	* 其中 l_addr 表示该模块（动态库或可执行文件）的加载基地址（即模块被加载到内存时的起始虚拟地址）。
	*/
	/*
	* 运行时虚拟地址 = 加载基地址 + 模块内偏移量（编译时确定）。
	* 因此，addr - link->l_addr 可得到该地址在模块（动态库 / 可执行文件）中的相对偏移量（编译时的地址，与调试信息中的地址一致）。
	*/
	return addr;
}

void* malloc(size_t size) 
{
	if (!malloc_f) {
		malloc_f = (malloc_t)dlsym(RTLD_NEXT, "malloc");
	}
	void* ptr = NULL;
	/*返回上一级调用函数的位置*/
	
	if (enable_malloc) {
		enable_malloc = 0;
		enable_free = 0;
		ptr = malloc_f(size);
		/*
		* n=0：返回当前函数的直接调用者的返回地址（即调用当前函数的指令的下一条指令地址）。
		* n=1：返回上一级调用者的返回地址（即调用当前函数的函数的调用者的返回地址），以此类推。
		*/
		void* caller = __builtin_return_address(0);
		const char* dir = "./mem_block";
		if (access(dir, F_OK) == -1) {
			if (mkdir(dir, 0755) == -1) {
				perror("mkdir failed");
				return NULL;
			}
		}

		char buff[128] = { 0 };
		snprintf(buff, sizeof(buff), "./mem_block/%p.mem", ptr);
		FILE* fp = fopen(buff, "w");
		if (!fp) {
			printf("fopen: %s failed\n", buff);
			free_f(ptr);
			return NULL;
		}
		fprintf(fp, "[+][%p] %p: %ld malloc\n", translate_address(caller), ptr, size);
		fflush(fp); /*强制刷新缓冲区，确保落盘*/
		fclose(fp);
		enable_malloc = 1;
		enable_free = 1;
	}
	else {
		ptr = malloc_f(size);
	}
	return ptr;
}

void free(void* ptr)
{
	if (ptr == NULL) {
		return;
	}

	if (!free_f) {
		free_f = (free_t)dlsym(RTLD_NEXT, "free");
	}
	
	if (enable_free) {
		enable_free = 0;
		enable_malloc = 0;
		char buff[128] = { 0 };
		snprintf(buff, sizeof(buff), "./mem_block/%p.mem", ptr);
		if (unlink(buff) < 0) {
			fprintf(stderr, "free error: unlink %s failed: ", buff);
			perror("");  // 打印具体错误（如" No such file or directory"）
			printf("double free: %p\n", ptr);
			return;
		}
		free_f(ptr);
		enable_free = 1;
		enable_malloc = 1;
	}
	else {
		free_f(ptr);
	}

	
}

#endif

int main(int argc, char** argv)
{
	void* p1 = malloc(5);
	void* p2 = malloc(10);
	void* p3 = malloc(15);

	free(p1);
	//free(p2);
	free(p3);
	return 0;
}