#pragma one
#include <unordered_map>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>


const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60; // 单位：秒

//线程对象
class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func);

	~Thread();

	//启动线程
	void start();

	int getId() const;
private:
	static int generaID_;
	int threadID;
	ThreadFunc func_;
};



// 线程池支持的模式
enum class PoolMode
{
	MODE_FIXED,  // 固定数量的线程
	MODE_CACHED, // 线程数量可动态增长
};

//Task对象前置声明
/*
这是一个基类，使用时需要用户重写run方法
*/
class Task
{
public:
	virtual	void run() = 0;
private:

};



class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool* operator= (const ThreadPool&) = delete;

	//启动线程池
	void start(int threadsize = std::thread::hardware_concurrency());

	// 设置线程池的工作模式
	void setMode(PoolMode mode);

	// 设置task任务队列上线阈值
	void setTaskQueMaxThreshHold(int threshhold);

	// 设置线程池cached模式下线程阈值
	void setThreadSizeThreshHold(int threshhold);

	// 给线程池提交任务
	void submitTask(std::shared_ptr<Task> sp);

private:
	// 定义线程函数
	void threadFunc(int threadid);

	// 检查pool的运行状态
	bool checkRunningState() const;
private:
	//线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> thread_list_;
	//当前线程数
	std::atomic_int currentThreadSize_;
	//空闲线程数
	std::atomic_int spareThreadSize_;
	//最大支持线程数
	int MaxThreadSize_;
	//默认线程数
	int initThreadSize_;

	//任务队列
	std::queue<std::shared_ptr<Task>> TaskQueue_;
	//当前任务数
	std::atomic_int currentTaskSize_;
	//任务数量最大阈值
	int MaxTaskSize_;

	//用于操作队列的锁
	std::mutex taskQueMtx_;
	//表示队列不满
	std::condition_variable notFull_;
	//表示队列不空
	std::condition_variable notEmpty_;

	//线程池工作模式
	PoolMode poolMode_;
	//线程池是否在工作
	std::atomic_bool isPoolRunning_;


};