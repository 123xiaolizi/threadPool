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
const int THREAD_MAX_IDLE_TIME = 60; // ��λ����

//�̶߳���
class Thread
{
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func);

	~Thread();

	//�����߳�
	void start();

	int getId() const;

private:
	static int generaID_;
	int threadID;
	ThreadFunc func_;
};

//������ڽ����������ݵ�����
class Any
{
public:
	Any() = default;
	~Any() = default;

	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;

	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//��Any���ͽ�����������������
	template<typename T>
	Any(T data): base_(std::make_unique<Derive<T>>(data))
	{}

	//��ȡdata����
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

	//��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	//������
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) : data_(data)
		{}
		T data_;  // �������������������
	};
private:
	// ����һ�������ָ��
	std::unique_ptr<Base> base_;
};


// ʵ��һ���ź�����
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
	{}
	~Semaphore() = default;

	// ��ȡһ���ź�����Դ
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		// �ȴ��ź�������Դ��û����Դ�Ļ�����������ǰ�߳�
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}

	// ����һ���ź�����Դ
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();  // �ȴ�״̬���ͷ�mutex�� ֪ͨ��������wait�ĵط�
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};


//Task����ǰ������
class Task;
// ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
class Result
{
public:

	Result(std::shared_ptr<Task> task, bool isValid = true);
	
	~Result() = default;

	//�������񷵻�ֵ
	void setVal(Any any);
	//��ȡ���񷵻�ֵ ----�û�����
	Any getVal();

private:
	//�������񷵻�ֵ
	Any any_;
	//�߳�ͨ���ź���
	Semaphore sem_;
	//ָ���Ӧ��ȡ����ֵ���������
	std::shared_ptr<Task> task_;
	//����ֵ�Ƿ���Ч
	std::atomic_bool isValid_;
};


/*
����һ�����࣬ʹ��ʱ��Ҫ�û���дrun����
*/
class Task
{
public:
	Task();
	~Task() = default;
	//���һ���м��ɣ��̳߳ص���
	void exec();
	//����result_���������������Ϣ���ͷ���ֵ
	void setResult(Result* res);

	//��Ҫ�û���д,����Ҫִ�е�����
	virtual	Any run() = 0;
private:
	Result* result_;
};


// �̳߳�֧�ֵ�ģʽ
enum class PoolMode
{
	MODE_FIXED,  // �̶��������߳�
	MODE_CACHED, // �߳������ɶ�̬����
};

class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool* operator= (const ThreadPool&) = delete;

	//�����̳߳�
	void start(int threadsize = std::thread::hardware_concurrency());

	// �����̳߳صĹ���ģʽ
	void setMode(PoolMode mode);

	// ����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshhold);

	// �����̳߳�cachedģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshhold);

	// ���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> sp);

private:
	// �����̺߳���
	void threadFunc(int threadid);

	// ���pool������״̬
	bool checkRunningState() const;
private:
	//�߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> thread_list_;
	//��ǰ�߳���
	std::atomic_int currentThreadSize_;
	//�����߳���
	std::atomic_int spareThreadSize_;
	//���֧���߳���
	int MaxThreadSize_;
	//Ĭ���߳���
	int initThreadSize_;

	//�������
	std::queue<std::shared_ptr<Task>> TaskQueue_;
	//��ǰ������
	std::atomic_int currentTaskSize_;
	//�������������ֵ
	int MaxTaskSize_;

	//���ڲ������е���
	std::mutex taskQueMtx_;
	//��ʾ���в���
	std::condition_variable notFull_;
	//��ʾ���в���
	std::condition_variable notEmpty_;
	// �ȵ��߳���Դȫ������
	std::condition_variable exitCond_; 

	//�̳߳ع���ģʽ
	PoolMode poolMode_;
	//�̳߳��Ƿ��ڹ���
	std::atomic_bool isPoolRunning_;


};