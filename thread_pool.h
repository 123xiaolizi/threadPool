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



// �̳߳�֧�ֵ�ģʽ
enum class PoolMode
{
	MODE_FIXED,  // �̶��������߳�
	MODE_CACHED, // �߳������ɶ�̬����
};

//Task����ǰ������
/*
����һ�����࣬ʹ��ʱ��Ҫ�û���дrun����
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

	//�����̳߳�
	void start(int threadsize = std::thread::hardware_concurrency());

	// �����̳߳صĹ���ģʽ
	void setMode(PoolMode mode);

	// ����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshhold);

	// �����̳߳�cachedģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshhold);

	// ���̳߳��ύ����
	void submitTask(std::shared_ptr<Task> sp);

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

	//�̳߳ع���ģʽ
	PoolMode poolMode_;
	//�̳߳��Ƿ��ڹ���
	std::atomic_bool isPoolRunning_;


};