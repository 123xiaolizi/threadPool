#pragma once
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

//这个用于接收任意数据的类型
class Any
{
public:
	Any() = default;
	~Any() = default;

	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;

	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//让Any类型接收任意其它的数据
	template<typename T>
	Any(T data): base_(std::make_unique<Derive<T>>(data))
	{}

	//获取data数据
	template<typename T>
	T cast_()
	{
		Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());
		if (ptr == nullptr)
		{
			throw "type is unmatch!";
		}
		return ptr->data_;
	}

private:

	//基类类型
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	//派生类
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) : data_(data)
		{}
		T data_;  // 保存了任意的其它类型
	};
private:
	// 定义一个基类的指针
	std::unique_ptr<Base> base_;
};


// 实现一个信号量类
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
	{}
	~Semaphore() = default;

	// 获取一个信号量资源
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		// 等待信号量有资源，没有资源的话，会阻塞当前线程
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}

	// 增加一个信号量资源
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();  // 等待状态，释放mutex锁 通知条件变量wait的地方
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};


//Task对象前置声明
class Task;
// 实现接收提交到线程池的task任务执行完成后的返回值类型Result
class Result
{
public:

	Result(std::shared_ptr<Task> task, bool isValid = true);
	
	~Result() = default;

	//设置任务返回值
	void setVal(Any any);
	//获取任务返回值 ----用户调用
	Any getVal();

private:
	//保存任务返回值
	Any any_;
	//线程通信信号量
	Semaphore sem_;
	//指向对应获取返回值的任务对象
	std::shared_ptr<Task> task_;
	//返回值是否有效
	std::atomic_bool isValid_;
};


/*
这是一个基类，使用时需要用户重写run方法
*/
class Task
{
public:
	Task();
	~Task() = default;
	//添加一个中间层吧，线程池调用
	void exec();
	//设置result_，保存任务对象信息，和返回值
	void setResult(Result* res);

	//需要用户重写,真正要执行的任务
	virtual	Any run() = 0;
private:
	Result* result_;
};


// 线程池支持的模式
enum class PoolMode
{
	MODE_FIXED,  // 固定数量的线程
	MODE_CACHED, // 线程数量可动态增长
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
	Result submitTask(std::shared_ptr<Task> sp);

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
	// 等到线程资源全部回收
	std::condition_variable exitCond_; 

	//线程池工作模式
	PoolMode poolMode_;
	//线程池是否在工作
	std::atomic_bool isPoolRunning_;


};