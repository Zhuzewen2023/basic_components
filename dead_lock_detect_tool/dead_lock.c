#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/*
* 想象有一个 "顶点列表"，每个顶点后面都跟着一个 "链表"，这个链表中存储的是从该顶点出发能直接到达的所有顶点（即邻接顶点）。
A → B → C  （A的邻接表：B和C）
B → C       （B的邻接表：C）
C → (空)    （C的邻接表：无）
*/

/*领接表节点结构*/
typedef struct AdjNode
{
	int dest;
	struct AdjNode* next;
} AdjNode;

/*领结表头结构*/
typedef struct AdjList
{
	AdjNode* head;
} AdjList;

typedef void* (*CopyDataFunc)(const void* data);
typedef void (*FreeDataFunc)(void* data);
typedef void (*PrintDataFunc)(const void* data);

/*有向图结构*/
typedef struct DirectedGraph
{
	int num_vertices; /*顶点数量*/
	AdjList* adj_lists; /*领接表数组*/
	void** vertex_data; /*顶点数据数组*/
	CopyDataFunc copy_data_function;
	FreeDataFunc free_data_function;
	PrintDataFunc print_data_function;
	//pthread_mutex_t graph_mutex; /*保护图操作的互斥锁*/
} DirectedGraph;

AdjNode* create_adjnode(int dest)
{
	AdjNode* new_node = (AdjNode*)malloc(sizeof(AdjNode));
	if (NULL == new_node) {
		printf("create adjnode failed, malloc failed\n");
		return NULL;
	}

	new_node->next = NULL;
	new_node->dest = dest;
	return new_node;
}

DirectedGraph* create_graph(int num_vertices, CopyDataFunc copy_func, FreeDataFunc free_func, PrintDataFunc print_func)
{
	DirectedGraph* graph = (DirectedGraph*)malloc(sizeof(DirectedGraph));
	if (NULL == graph) {
		printf("create graph failed, malloc failed\n");
		return NULL;
	}

	graph->num_vertices = num_vertices;
	graph->copy_data_function = copy_func;
	graph->free_data_function = free_func;
	graph->print_data_function = print_func;

	/*初始化领接表*/
	graph->adj_lists = (AdjList*)malloc(num_vertices * sizeof(AdjList));
	if (NULL == graph->adj_lists) {
		printf("malloc adj_lists failed\n");
		free(graph);
		return NULL;
	}

	/*初始化顶点数据数组*/
	graph->vertex_data = (void**)malloc(num_vertices * sizeof(void*));
	if (NULL == graph->vertex_data) {
		printf("malloc vertex_data failed\n");
		free(graph->adj_lists);
		free(graph);
		return NULL;
	}

	for (int i = 0; i < num_vertices; i++) {
		graph->adj_lists[i].head = NULL;
		graph->vertex_data[i] = NULL;
	}

	//pthread_mutex_init(&graph->graph_mutex, NULL);

	return graph;
}

bool set_vertex_data(DirectedGraph* graph, int vertex, const void* data)
{
	if (NULL == graph || vertex < 0 || vertex >= graph->num_vertices || NULL == data) {
		printf("invalid param\n");
		return false;
	}
	//pthread_mutex_lock(&graph->graph_mutex);

	if (NULL != graph->vertex_data[vertex]) {
		graph->free_data_function(graph->vertex_data[vertex]);
	}

	graph->vertex_data[vertex] = graph->copy_data_function(data);
	//pthread_mutex_unlock(&graph->graph_mutex);
	return NULL != graph->vertex_data[vertex];
}

void add_edge(DirectedGraph* graph, int src, int dest)
{
	if (src < 0 || src >= graph->num_vertices || dest < 0 || dest >= graph->num_vertices) {
		printf("invalid src or dest\n");
		return;
	}

	//pthread_mutex_lock(&graph->graph_mutex);

	AdjNode* new_node = create_adjnode(dest);
	/*头插法*/
	new_node->next = graph->adj_lists[src].head;
	graph->adj_lists[src].head = new_node;

	//pthread_mutex_unlock(&graph->graph_mutex);
}

bool remove_edge(DirectedGraph* graph, int src, int dest)
{
	if (src < 0 || src >= graph->num_vertices || dest < 0 || dest >= graph->num_vertices) {
		printf("invalid src or dest\n");
		return false;
	}

	//pthread_mutex_lock(&graph->graph_mutex);

	AdjNode* original_src_head = graph->adj_lists[src].head;
	AdjNode* prev = NULL;
	if (NULL != original_src_head && original_src_head->dest == dest) {
		/*头节点就是要删除的节点*/
		graph->adj_lists[src].head = original_src_head->next;
		free(original_src_head);
		//pthread_mutex_unlock(&graph->graph_mutex);
		return true;
	}

	while (NULL != original_src_head && original_src_head->dest != dest) {
		prev = original_src_head;
		original_src_head = original_src_head->next;
	}

	if (NULL == original_src_head) {
		/*没找到节点*/
		//pthread_mutex_unlock(&graph->graph_mutex);
		return false;
	}

	/*找到节点*/
	prev->next = original_src_head->next;
	free(original_src_head);
	//pthread_mutex_unlock(&graph->graph_mutex);
	return true;
}

/*删除顶点*/
bool remove_vertex(DirectedGraph* graph, int vertex)
{
	if (vertex < 0 || vertex >= graph->num_vertices) {
		printf("invalid vertex\n");
		return false;
	}
	//pthread_mutex_lock(&graph->graph_mutex);

	if (NULL != graph->vertex_data[vertex]) {
		graph->free_data_function(graph->vertex_data[vertex]);
		graph->vertex_data[vertex] = NULL;
	}

	/*删除从顶点出发的所有边*/
	AdjNode* current = graph->adj_lists[vertex].head;
	while (NULL != current) {
		AdjNode* next = current->next;
		free(current);
		current = next;
	}
	graph->adj_lists[vertex].head = NULL;
	/*删除指向该顶点的所有边*/
	for (int i = 0; i < graph->num_vertices; i++) {
		if (i != vertex) {
			remove_edge(graph, i, vertex);
		}
	}

	//pthread_mutex_unlock(&graph->graph_mutex);
	return true;
}

void dfs_util(DirectedGraph* graph, int vertex, bool visited[])
{
	visited[vertex] = true;
	printf("vertex data: ");
	graph->print_data_function(graph->vertex_data[vertex]);
	printf(" -> ");

	AdjNode* current = graph->adj_lists[vertex].head;
	while (NULL != current) {
		int adj_vertex = current->dest;
		if (visited[adj_vertex] != true) {
			dfs_util(graph, adj_vertex, visited);
		}
		current = current->next;
	}
}

/*深度优先搜索*/
void dfs(DirectedGraph* graph, int start_vertex)
{
	if (start_vertex < 0 || start_vertex >= graph->num_vertices) {
		printf("invalid vertex\n");
		return;
	}

	bool* visited = (bool*)malloc(graph->num_vertices * sizeof(bool));
	if (NULL == visited) {
		printf("malloc visited array failed\n");
		return;
	}

	memset(visited, 0, graph->num_vertices * sizeof(bool));
	printf("DFS: ");
	//pthread_mutex_lock(&graph->graph_mutex);
	dfs_util(graph, start_vertex, visited);
	//pthread_mutex_unlock(&graph->graph_mutex);
	printf("\n");
}

void free_graph(DirectedGraph* graph)
{
	if (NULL == graph) {
		return;
	}

	/*释放所有顶点数据*/
	for (int i = 0; i < graph->num_vertices; ++i) {
		if (NULL != graph->vertex_data[i]) {
			graph->free_data_function(graph->vertex_data[i]);
		}
	}

	/*释放所有领接表节点*/
	for (int i = 0; i < graph->num_vertices; ++i) {
		AdjNode* current = graph->adj_lists[i].head;
		while (NULL != current) {
			AdjNode* next = current->next;
			free(current);
			current = next;
		}
	}
	//pthread_mutex_destroy(&graph->graph_mutex);
	free(graph->adj_lists);
	free(graph->vertex_data);
	free(graph);
}

static bool search_cycle_util(DirectedGraph* graph, int vertex, bool visited[], bool rec_stack[])
{
	if (NULL == graph || vertex >= graph->num_vertices || NULL == visited || NULL == rec_stack) {
		printf("invalid param\n");
		return false;
	}
	/*找到起始节点*/
	AdjNode* curr_node = graph->adj_lists[vertex].head;
	/*标记起始节点已经访问*/
	visited[vertex] = true;
	/*路径中加入该起始节点*/
	rec_stack[vertex] = true;
	while (NULL != curr_node) {
		int adj_vertex = curr_node->dest;
		if (!visited[adj_vertex]) {
			if (search_cycle_util(graph, adj_vertex, visited, rec_stack)) {
				return true;
			}
		}
		else if (rec_stack[adj_vertex]) {
			return true;
		}
		curr_node = curr_node->next;
	}
	rec_stack[vertex] = false;
	return false;
	
}

bool search_cycle(DirectedGraph* graph, int start_vertex)
{
	/*参数检查*/
	if (NULL == graph || start_vertex >= graph->num_vertices) {
		printf("invalid params\n");
		return false;
	}

	/*全局访问数组*/
	bool* visited = (bool*)malloc(graph->num_vertices * sizeof(bool));
	if (NULL == visited) {
		printf("malloc visited arr failed\n");
		return false;
	}

	/*递归路径数组*/
	bool* rec_stack = (bool*)malloc(graph->num_vertices * sizeof(bool));
	if (NULL == rec_stack) {
		printf("malloc rec_stack arr failed\n");
		free(visited);
		return false;
	}

	memset(visited, 0, graph->num_vertices * sizeof(bool));
	memset(rec_stack, 0, graph->num_vertices * sizeof(bool));
	//pthread_mutex_lock(&graph->graph_mutex);
	bool has_cycle = search_cycle_util(graph, start_vertex, visited, rec_stack);
	//pthread_mutex_unlock(&graph->graph_mutex);
	free(visited);
	free(rec_stack);
	return has_cycle;

}


#if 0
// 示例1：存储字符串数据
void* copy_string(const void* data) {
	const char* str = (const char*)data;
	char* new_str = (char*)malloc(strlen(str) + 1);
	if (new_str) strcpy(new_str, str);
	return new_str;
}

void free_string(void* data) {
	free(data);  // 字符串是malloc分配的，直接free
}

void print_string(const void* data) {
	printf("\"%s\"", (const char*)data);  // 打印字符串带引号
}

// 示例2：存储自定义结构体（如人员信息）
typedef struct {
	char name[20];
	int age;
} Person;

void* copy_person(const void* data) {
	const Person* p = (const Person*)data;
	Person* new_p = (Person*)malloc(sizeof(Person));
	if (new_p) *new_p = *p;  // 复制结构体
	return new_p;
}

void free_person(void* data) {
	free(data);  // 结构体是malloc分配的，直接free
}

void print_person(const void* data) {
	const Person* p = (const Person*)data;
	printf("{name: %s, age: %d}", p->name, p->age);  // 打印结构体内容
}

// 测试工具函数：打印单个测试结果
void test_case(const char* case_name, bool expected, bool actual) {
	printf("%-40s %s\n", case_name, (expected == actual) ? "PASS" : "FAIL");
}

// 测试1：无环图（A→B→C→D）
void test_no_cycle() {
	DirectedGraph* graph = create_graph(4, copy_string, free_string, print_string);
	set_vertex_data(graph, 0, "A");
	set_vertex_data(graph, 1, "B");
	set_vertex_data(graph, 2, "C");
	set_vertex_data(graph, 3, "D");

	add_edge(graph, 0, 1);  // A→B
	add_edge(graph, 1, 2);  // B→C
	add_edge(graph, 2, 3);  // C→D

	bool result = search_cycle(graph, 0);
	test_case("无环图（A→B→C→D）", false, result);

	free_graph(graph);
}

// 测试2：大环（A→B→C→D→A）
void test_large_cycle() {
	DirectedGraph* graph = create_graph(4, copy_string, free_string, print_string);
	set_vertex_data(graph, 0, "A");
	set_vertex_data(graph, 1, "B");
	set_vertex_data(graph, 2, "C");
	set_vertex_data(graph, 3, "D");

	add_edge(graph, 0, 1);  // A→B
	add_edge(graph, 1, 2);  // B→C
	add_edge(graph, 2, 3);  // C→D
	add_edge(graph, 3, 0);  // D→A（形成环）

	bool result = search_cycle(graph, 0);
	test_case("大环（A→B→C→D→A）", true, result);

	free_graph(graph);
}

// 测试3：中间环（A→B→C→D→B）
void test_middle_cycle() {
	DirectedGraph* graph = create_graph(4, copy_string, free_string, print_string);
	set_vertex_data(graph, 0, "A");
	set_vertex_data(graph, 1, "B");
	set_vertex_data(graph, 2, "C");
	set_vertex_data(graph, 3, "D");

	add_edge(graph, 0, 1);  // A→B
	add_edge(graph, 1, 2);  // B→C
	add_edge(graph, 2, 3);  // C→D
	add_edge(graph, 3, 1);  // D→B（形成环）

	bool result = search_cycle(graph, 0);
	test_case("中间环（A→B→C→D→B）", true, result);

	free_graph(graph);
}

// 测试4：分支环（A→B→C→D→E→F，E→C）
void test_branch_cycle() {
	DirectedGraph* graph = create_graph(6, copy_string, free_string, print_string);
	set_vertex_data(graph, 0, "A");
	set_vertex_data(graph, 1, "B");
	set_vertex_data(graph, 2, "C");
	set_vertex_data(graph, 3, "D");
	set_vertex_data(graph, 4, "E");
	set_vertex_data(graph, 5, "F");

	add_edge(graph, 0, 1);  // A→B
	add_edge(graph, 1, 2);  // B→C
	add_edge(graph, 2, 3);  // C→D
	add_edge(graph, 3, 4);  // D→E
	add_edge(graph, 4, 5);  // E→F
	add_edge(graph, 4, 2);  // E→C（形成环）

	bool result = search_cycle(graph, 0);
	test_case("分支环（A→B→C→D→E→F，E→C）", true, result);

	free_graph(graph);
}

// 测试5：自环（A→A）
void test_self_cycle() {
	DirectedGraph* graph = create_graph(1, copy_string, free_string, print_string);
	set_vertex_data(graph, 0, "A");

	add_edge(graph, 0, 0);  // A→A（自环）

	bool result = search_cycle(graph, 0);
	test_case("自环（A→A）", true, result);

	free_graph(graph);
}

// 测试6：不可达环（A→B，C→D→C）
void test_unreachable_cycle() {
	DirectedGraph* graph = create_graph(4, copy_string, free_string, print_string);
	set_vertex_data(graph, 0, "A");
	set_vertex_data(graph, 1, "B");
	set_vertex_data(graph, 2, "C");
	set_vertex_data(graph, 3, "D");

	add_edge(graph, 0, 1);  // A→B（无环）
	add_edge(graph, 2, 3);  // C→D
	add_edge(graph, 3, 2);  // D→C（形成环，但从A不可达）

	bool result = search_cycle(graph, 0);  // 从A出发检测
	test_case("不可达环（A无法到达C→D→C）", false, result);

	free_graph(graph);
}

// 运行所有测试
void run_all_cycle_tests() {
	printf("===== 有向图环检测测试 =====\n");
	test_no_cycle();
	test_self_cycle();
	test_large_cycle();
	test_middle_cycle();
	test_branch_cycle();
	test_unreachable_cycle();
}

// 测试函数
int main() {
	// 测试1：存储字符串数据
	printf("===== 测试存储字符串 =====\n");
	DirectedGraph* str_graph = create_graph(3, copy_string, free_string, print_string);
	set_vertex_data(str_graph, 0, "A");  // 顶点0存储字符串"A"
	set_vertex_data(str_graph, 1, "B");
	set_vertex_data(str_graph, 2, "C");
	add_edge(str_graph, 0, 1);  // A->B
	add_edge(str_graph, 0, 2);  // A->C
	dfs(str_graph, 0);  // 遍历会打印字符串
	free_graph(str_graph);

	// 测试2：存储自定义结构体
	printf("\n===== 测试存储结构体 =====\n");
	DirectedGraph* person_graph = create_graph(2, copy_person, free_person, print_person);
	Person p1 = { "Alice", 25 };
	Person p2 = { "Bob", 30 };
	set_vertex_data(person_graph, 0, &p1);  // 顶点0存储p1
	set_vertex_data(person_graph, 1, &p2);  // 顶点1存储p2
	add_edge(person_graph, 0, 1);  // Alice->Bob
	dfs(person_graph, 0);  // 遍历会打印结构体内容
	free_graph(person_graph);

	/*该如何测试环检查的函数*/
	/*创造一个环，需要创造哪些环?*/
	/*A->B->C->D->A*/
	/*A->B->C->D->B*/
	/*A->B->C->D->E->F
	E->C
	*/
	/*还需要创造一个无环*/
	/*A->B->C->D*/
	printf("\n===== 测试环检测 =====\n");
	run_all_cycle_tests();

	return 0;
}
#endif

typedef int (*pthread_create_t)(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine) (void*), void* arg);
pthread_create_t pthread_create_f = NULL;

typedef int (*pthread_mutex_lock_t)(pthread_mutex_t* mtx);
pthread_mutex_lock_t pthread_mutex_lock_f = NULL;

typedef int (*pthread_mutex_unlock_t)(pthread_mutex_t* mtx);
pthread_mutex_unlock_t pthread_mutex_unlock_f = NULL;

DirectedGraph* g_graph = NULL;

#define MAX_VERTICES	100
int g_next_vertex = 0;
pthread_mutex_t g_vertex_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef enum
{
	VERTEX_TYPE_THREAD,
	VERTEX_TYPE_MUTEX,
}VertexType;

typedef struct
{
	VertexType type;
	union {
		pthread_t thread_id;
		uintptr_t mutex_addr;
	} id;
} VertexData;

// 初始化控制
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static volatile int g_init_complete = 0;  // 初始化完成标志

// 调试宏
#define DEBUG_PRINT(fmt, ...) \
    do { \
        printf("[DEBUG] %s:%d " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        fflush(stdout); \
    } while(0)



void* copy_vertex_data(const void* data)
{
	const VertexData* src = (const VertexData*)data;
	VertexData* dest = (VertexData*)malloc(sizeof(VertexData));
	if (dest) {
		*dest = *src;
	}
	return dest;
}

void free_vertex_data(void* data)
{
	free(data);
}

void print_vertex_data(const void* data)
{
	const VertexData* vd = (const VertexData*)data;
	if (vd->type == VERTEX_TYPE_MUTEX) {
		printf("Mutex %p", (void*)vd->id.mutex_addr);
	}
	else {
		printf("Thread %ld", (unsigned long)vd->id.thread_id);
	}
}


static void init_hook_once(void)
{
	DEBUG_PRINT("开始初始化钩子函数");

	if (!pthread_create_f) {
		pthread_create_f = dlsym(RTLD_NEXT, "pthread_create");
		if (!pthread_create_f) {
			DEBUG_PRINT("获取 pthread_create 失败: %s", dlerror());
		}
	}

	if (!pthread_mutex_lock_f) {
		pthread_mutex_lock_f = dlsym(RTLD_NEXT, "pthread_mutex_lock");
		if (!pthread_mutex_lock_f) {
			DEBUG_PRINT("获取 pthread_mutex_lock 失败: %s", dlerror());
		}
	}

	if (!pthread_mutex_unlock_f) {
		pthread_mutex_unlock_f = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
		if (!pthread_mutex_unlock_f) {
			DEBUG_PRINT("获取 pthread_mutex_unlock 失败: %s", dlerror());
		}
	}

	// 初始化图结构
	if (!g_graph) {
		DEBUG_PRINT("开始创建图结构");
		g_graph = create_graph(MAX_VERTICES, copy_vertex_data, free_vertex_data, print_vertex_data);
		if (!g_graph) {
			DEBUG_PRINT("创建图结构失败!");
			return;
		}
		DEBUG_PRINT("图结构创建成功: %p", g_graph);
	}

	g_init_complete = 1;  // 标记初始化完成
	DEBUG_PRINT("钩子函数初始化完成");
}

static int ensure_hooks_initialized(void)
{
	pthread_once(&init_once, init_hook_once);

	// 等待初始化完成
	while (!g_init_complete) {
		usleep(1000);  // 等待1ms
	}

	// 验证关键组件已初始化
	if (!g_graph || !pthread_mutex_lock_f || !pthread_mutex_unlock_f) {
		DEBUG_PRINT("初始化失败: g_graph=%p, lock_f=%p, unlock_f=%p",
			g_graph, pthread_mutex_lock_f, pthread_mutex_unlock_f);
		return -1;
	}

	return 0;
}



int find_vertex(DirectedGraph* graph, const VertexData* data)
{
	//printf("enter find vertex\n");
	if (!graph || !data) {
		printf("graph == NULL || data == NULL\n");
		return -1;
	}
	//pthread_mutex_lock(&graph->graph_mutex);
	printf("g_next_vertex:%d\n", g_next_vertex);
	for (int i = 0; i < g_next_vertex; i++) {
		VertexData* vd = (VertexData*)graph->vertex_data[i];
		if (vd && vd->type == data->type) {
			if ((vd->type == VERTEX_TYPE_THREAD && vd->id.thread_id == data->id.thread_id) ||
				(vd->type == VERTEX_TYPE_MUTEX && vd->id.mutex_addr == data->id.mutex_addr)) {
				//pthread_mutex_unlock(&graph->graph_mutex);
				return i;
			}
		}
	}

	//pthread_mutex_unlock(&graph->graph_mutex);
	return -1;
}

int get_or_create_vertex(DirectedGraph* graph, const VertexData* data)
{
	DEBUG_PRINT("get_or_create_vertex 被调用");

	if (!graph || !data) {
		DEBUG_PRINT("参数为空: graph=%p, data=%p", graph, data);
		return -1;
	}

	if (g_next_vertex >= MAX_VERTICES) {
		DEBUG_PRINT("顶点数量已达上限: %d", g_next_vertex);
		return -1;
	}

	// 检查图结构完整性
	if (!graph->adj_lists || !graph->vertex_data) {
		DEBUG_PRINT("图结构损坏");
		return -1;
	}

	int index = find_vertex(graph, data);
	if (index != -1) {
		DEBUG_PRINT("找到已存在顶点: %d", index);
		return index;
	}

	DEBUG_PRINT("创建新顶点");
	//pthread_mutex_lock(&g_vertex_mutex);

	// 再次检查，避免竞态条件
	if (g_next_vertex >= MAX_VERTICES) {
		//pthread_mutex_unlock(&g_vertex_mutex);
		DEBUG_PRINT("创建顶点时达到上限");
		return -1;
	}

	index = g_next_vertex++;
	//pthread_mutex_unlock(&g_vertex_mutex);

	DEBUG_PRINT("分配到顶点索引: %d", index);

	if (!set_vertex_data(graph, index, data)) {
		DEBUG_PRINT("设置顶点数据失败");
		// 回滚
		//pthread_mutex_lock(&g_vertex_mutex);
		g_next_vertex--;
		//pthread_mutex_unlock(&g_vertex_mutex);
		return -1;
	}

	DEBUG_PRINT("顶点创建成功: %d", index);
	return index;
}



#if 1
/*原语操作*/
void before_lock(pthread_t tid, pthread_mutex_t* mtx)
{
	DEBUG_PRINT("线程 %ld 准备获取锁 %p", (unsigned long)tid, mtx);

	// 严格检查参数和初始化状态
	if (!mtx) {
		DEBUG_PRINT("mtx 为 NULL");
		return;
	}

	if (!g_init_complete) {
		DEBUG_PRINT("初始化未完成，跳过");
		return;
	}

	if (!g_graph) {
		DEBUG_PRINT("g_graph 为 NULL");
		return;
	}

	// 验证 g_graph 的基本完整性
	if (g_graph->num_vertices <= 0 || g_graph->num_vertices > MAX_VERTICES) {
		DEBUG_PRINT("g_graph 损坏: num_vertices = %d", g_graph->num_vertices);
		return;
	}

	if (!g_graph->adj_lists || !g_graph->vertex_data) {
		DEBUG_PRINT("g_graph 内部结构损坏: adj_lists=%p, vertex_data=%p",
			g_graph->adj_lists, g_graph->vertex_data);
		return;
	}

	VertexData thread_data = { .type = VERTEX_TYPE_THREAD, .id.thread_id = tid };
	VertexData mutex_data = { .type = VERTEX_TYPE_MUTEX, .id.mutex_addr = (uintptr_t)mtx };

	DEBUG_PRINT("调用 get_or_create_vertex for thread");
	int thread_vertex = get_or_create_vertex(g_graph, &thread_data);
	if (thread_vertex == -1) {
		DEBUG_PRINT("创建线程顶点失败");
		return;
	}

	DEBUG_PRINT("调用 get_or_create_vertex for mutex");
	int mutex_vertex = get_or_create_vertex(g_graph, &mutex_data);
	if (mutex_vertex == -1) {
		DEBUG_PRINT("创建锁顶点失败");
		return;
	}

	DEBUG_PRINT("thread_vertex: %d, mutex_vertex: %d", thread_vertex, mutex_vertex);

	// 添加边之前再次检查
	if (thread_vertex >= 0 && mutex_vertex >= 0 &&
		thread_vertex < g_graph->num_vertices && mutex_vertex < g_graph->num_vertices) {
		DEBUG_PRINT("添加边: %d -> %d", thread_vertex, mutex_vertex);
		add_edge(g_graph, thread_vertex, mutex_vertex);
	}
	else {
		DEBUG_PRINT("顶点索引无效: thread=%d, mutex=%d, max=%d",
			thread_vertex, mutex_vertex, g_graph->num_vertices);
	}
}

/*原语操作*/
void after_lock(pthread_t tid, pthread_mutex_t* mtx)
{
	//printf("enter after lock\n");
	if (!g_graph) return;
	// 创建线程和锁的顶点数据
	VertexData thread_data = { .type = VERTEX_TYPE_THREAD, .id.thread_id = tid };
	VertexData mutex_data = { .type = VERTEX_TYPE_MUTEX, .id.mutex_addr = (uintptr_t)mtx };

	// 获取顶点
	int thread_vertex = find_vertex(g_graph, &thread_data);
	int mutex_vertex = find_vertex(g_graph, &mutex_data);

	if (thread_vertex != -1 && mutex_vertex != -1) {
		// 移除线程→锁的边（等待关系）
		remove_edge(g_graph, thread_vertex, mutex_vertex);
		// 添加锁→线程的边（持有关系）
		add_edge(g_graph, mutex_vertex, thread_vertex);
	}
	
}

/*原语操作*/
void before_unlock(pthread_t tid, pthread_mutex_t* mtx)
{

}

/*原语操作*/
void after_unlock(pthread_t tid, pthread_mutex_t* mtx)
{
	if (!g_graph) return;
	// 创建线程和锁的顶点数据
	VertexData thread_data = { .type = VERTEX_TYPE_THREAD, .id.thread_id = tid };
	VertexData mutex_data = { .type = VERTEX_TYPE_MUTEX, .id.mutex_addr = (uintptr_t)mtx };

	// 获取顶点
	int thread_vertex = find_vertex(g_graph, &thread_data);
	int mutex_vertex = find_vertex(g_graph, &mutex_data);

	if (thread_vertex != -1 && mutex_vertex != -1) {
		// 移除锁→线程的边（持有关系）
		remove_edge(g_graph, mutex_vertex, thread_vertex);
	}
}



int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine) (void*), void* arg)
{
	int result = pthread_create_f(thread, attr, start_routine, arg);
	if (0 == result && g_graph && thread) {
		VertexData thread_data = {
			.type = VERTEX_TYPE_THREAD,
			.id.thread_id = *thread
		};
		get_or_create_vertex(g_graph, &thread_data);
	}
	return result;
}

int pthread_mutex_lock(pthread_mutex_t* mtx)
{
	//printf("before pthread_mutex_lock %ld, %p\n", pthread_self(), mtx);
	pthread_t tid = pthread_self();
	before_lock(tid, mtx);
	int result = pthread_mutex_lock_f(mtx);
	if (0 == result) {
		after_lock(tid, mtx);
	}
	return result;
	//printf("after pthread_mutex_lock\n");
}

int pthread_mutex_unlock(pthread_mutex_t* mtx)
{
	//printf("before pthread_mutex_unlock %ld, %p\n", pthread_self(), mtx);
	pthread_t tid = pthread_self();
	before_unlock(tid, mtx);
	int result = pthread_mutex_unlock_f(mtx);
	if (result == 0) {
		after_unlock(tid, mtx);
	}
	return result;
	//printf("after pthread_mutex_unlock\n");
}

void init_hook(void)
{
	/*dlsym 是动态链接库的符号解析函数，用于在运行时查找共享库中的函数或变量地址。
	RTLD_NEXT 是一个特殊的标志，表示 “在当前库之后的搜索顺序中查找符号”—— 即跳过当前代码（可能包含钩子函数），
	找到系统原生的 pthread_mutex_lock 函数。*/
	if (!pthread_create_f) {
		pthread_create_f = dlsym(RTLD_NEXT, "pthread_create");
	}

	if (!pthread_mutex_lock_f) {
		pthread_mutex_lock_f = dlsym(RTLD_NEXT, "pthread_mutex_lock");
	}

	if (!pthread_mutex_unlock_f) {
		pthread_mutex_unlock_f = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
	}
}

#if 1 /*debug*/

pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx4 = PTHREAD_MUTEX_INITIALIZER;

// 线程1：锁定mtx1，然后尝试锁定mtx2
void* t1_cb(void* arg)
{
	printf("线程1: 尝试锁定mtx1\n");
	pthread_mutex_lock(&mtx1);
	printf("线程1: 已锁定mtx1\n");

	sleep(1);  // 给其他线程获取锁的机会

	printf("线程1: 尝试锁定mtx2\n");
	pthread_mutex_lock(&mtx2);
	printf("线程1: 已锁定mtx2\n");

	pthread_mutex_unlock(&mtx2);
	printf("线程1: 已解锁mtx2\n");
	pthread_mutex_unlock(&mtx1);
	printf("线程1: 已解锁mtx1\n");

	return NULL;
}

// 线程2：锁定mtx2，然后尝试锁定mtx3
void* t2_cb(void* arg)
{
	printf("线程2: 尝试锁定mtx2\n");
	pthread_mutex_lock(&mtx2);
	printf("线程2: 已锁定mtx2\n");

	sleep(1);  // 给其他线程获取锁的机会

	printf("线程2: 尝试锁定mtx3\n");
	pthread_mutex_lock(&mtx3);
	printf("线程2: 已锁定mtx3\n");

	pthread_mutex_unlock(&mtx3);
	printf("线程2: 已解锁mtx3\n");
	pthread_mutex_unlock(&mtx2);
	printf("线程2: 已解锁mtx2\n");

	return NULL;
}

// 线程3：锁定mtx3，然后尝试锁定mtx4
void* t3_cb(void* arg)
{
	printf("线程3: 尝试锁定mtx3\n");
	pthread_mutex_lock(&mtx3);
	printf("线程3: 已锁定mtx3\n");

	sleep(1);  // 给其他线程获取锁的机会

	printf("线程3: 尝试锁定mtx4\n");
	pthread_mutex_lock(&mtx4);
	printf("线程3: 已锁定mtx4\n");

	pthread_mutex_unlock(&mtx4);
	printf("线程3: 已解锁mtx4\n");
	pthread_mutex_unlock(&mtx3);
	printf("线程3: 已解锁mtx3\n");

	return NULL;
}

// 线程4：锁定mtx4，然后尝试锁定mtx1（形成环）
void* t4_cb(void* arg)
{
	printf("线程4: 尝试锁定mtx4\n");
	pthread_mutex_lock(&mtx4);
	printf("线程4: 已锁定mtx4\n");

	sleep(1);  // 给其他线程获取锁的机会

	printf("线程4: 尝试锁定mtx1\n");
	pthread_mutex_lock(&mtx1);
	printf("线程4: 已锁定mtx1\n");

	pthread_mutex_unlock(&mtx1);
	printf("线程4: 已解锁mtx1\n");
	pthread_mutex_unlock(&mtx4);
	printf("线程4: 已解锁mtx4\n");

	return NULL;
}

// 可选：单独的环检测函数，由监控线程定期调用
void* deadlock_monitor_thread(void* arg)
{
	while (1) {
		sleep(1); // 每秒检测一次

		if (!g_graph) continue;

		// 对所有活跃线程检测环
		//pthread_mutex_lock(&g_graph->graph_mutex);
		for (int i = 0; i < g_next_vertex; i++) {
			VertexData* vd = (VertexData*)g_graph->vertex_data[i];
			if (vd && vd->type == VERTEX_TYPE_THREAD) {
				// 创建临时的visited数组进行检测
				bool* visited = (bool*)calloc(g_graph->num_vertices, sizeof(bool));
				bool* rec_stack = (bool*)calloc(g_graph->num_vertices, sizeof(bool));

				if (visited && rec_stack) {
					if (search_cycle_util(g_graph, i, visited, rec_stack)) {
						printf("检测到潜在死锁，涉及线程 %ld\n",
							(unsigned long)vd->id.thread_id);
					}
				}

				free(visited);
				free(rec_stack);
			}
		}
		//pthread_mutex_unlock(&g_graph->graph_mutex);
	}
	return NULL;
}

// 修复后的 main 函数
int main()
{
	DEBUG_PRINT("程序开始");

	// 确保初始化
	if (ensure_hooks_initialized() != 0) {
		fprintf(stderr, "初始化失败\n");
		return 1;
	}

	DEBUG_PRINT("初始化完成，开始创建线程");

	pthread_t t1, t2, t3, t4, monitor;

	// 创建监控线程
	if (pthread_create(&monitor, NULL, deadlock_monitor_thread, NULL) != 0) {
		fprintf(stderr, "创建监控线程失败\n");
		return 1;
	}

	// 创建工作线程
	pthread_create(&t1, NULL, t1_cb, NULL);
	//sleep(1); // 给每个线程一些启动时间
	pthread_create(&t2, NULL, t2_cb, NULL);
	//sleep(1);
	pthread_create(&t3, NULL, t3_cb, NULL);
	//sleep(1);
	pthread_create(&t4, NULL, t4_cb, NULL);

	// 等待工作线程完成
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_join(t3, NULL);
	pthread_join(t4, NULL);

	// 取消监控线程
	pthread_cancel(monitor);

	DEBUG_PRINT("所有线程已完成");

	if (g_graph) {
		free_graph(g_graph);
		g_graph = NULL;
	}

	pthread_mutex_destroy(&g_vertex_mutex);
	return 0;
}
#endif

#endif