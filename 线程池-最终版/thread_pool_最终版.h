#pragma once
#pragma once
#pragma once
#include <unordered_map>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <iostream>
#include <future>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60; // ��λ����

//�̶߳���
class Thread
{
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func) :func_(func), threadID(generaID_++)
	{}

	~Thread() = default;

	//�����߳�
	void start()
	{
		std::thread t(func_, threadID);
		std::cout << "create thread" << t.get_id() << std::endl;
		t.detach();
	}

	int getId() const
	{
		return threadID;
	}

private:
	static int generaID_;
	int threadID;
	ThreadFunc func_;
};

int Thread::generaID_ = 0;

// �̳߳�֧�ֵ�ģʽ
enum class PoolMode
{
	MODE_FIXED,  // �̶��������߳�
	MODE_CACHED, // �߳������ɶ�̬����
};

class ThreadPool
{
public:
	ThreadPool() :
		currentTaskSize_(0),
		spareThreadSize_(0),
		MaxThreadSize_(THREAD_MAX_THRESHHOLD),
		MaxTaskSize_(TASK_MAX_THRESHHOLD),
		initThreadSize_(0),
		currentThreadSize_(0),
		poolMode_(PoolMode::MODE_FIXED),
		isPoolRunning_(false)
	{}
	~ThreadPool()
	{
		isPoolRunning_ = false;
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool {return thread_list_.size() == 0; });
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool* operator= (const ThreadPool&) = delete;

	//�����̳߳�
	void start(int threadsize = std::thread::hardware_concurrency())
	{
		//�����̳߳�Ϊ����״̬
		isPoolRunning_ = true;
		initThreadSize_ = threadsize;
		currentThreadSize_ = threadsize;
		//����thread����
		for (int i = 0; i < threadsize && i < MaxThreadSize_; ++i)
		{
			auto thread_ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int thread_id = thread_ptr->getId();
			thread_list_.emplace(thread_id, std::move(thread_ptr));
		}
		for (int i = 0; i < thread_list_.size(); ++i)
		{
			thread_list_[i]->start();
			++spareThreadSize_;
		}
	}

	// �����̳߳صĹ���ģʽ
	void setMode(PoolMode mode)
	{
		poolMode_ = mode;
	}

	// ����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshhold)
	{
		MaxTaskSize_ = threshhold;
	}

	// �����̳߳�cachedģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshhold)
	{
		MaxThreadSize_ = threshhold;
	}

	// ���̳߳��ύ����
	//Result submitTask(std::shared_ptr<Task> sp);
	template<typename Func, typename... Args>       //�Ƶ�����ֵ����
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		//�������
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			//ʹ��forward����ת�������ֲ���������ֵ����
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		//get_future() ������������ֻ�Ƿ���һ���� std::packaged_task ������ std::future ����
		std::future<RType> result = task->get_future();

		std::unique_lock<std::mutex> lock(taskQueMtx_);
		//�ύ��������ʱ������1��
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool {return TaskQueue_.size() < MaxTaskSize_; }))
		{
			std::cerr << "task queue is full, submit task fail.---- time_out" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>> (
				[]()->RType {return RType(); });
			(*task)();
			return task->get_future();
		}
		//�ߵ�����˵����������л��пռ�
		TaskQueue_.emplace([task]() {(*task)(); });
		currentTaskSize_++;

		//֪ͨ�����߳�
		notEmpty_.notify_all();

		// cachedģʽ ������ȽϽ���  �ж��Ƿ���Ҫ�����߳�
		if (poolMode_ == PoolMode::MODE_CACHED
			&& spareThreadSize_ < currentTaskSize_	//�����߳�С��������
			&& currentThreadSize_ < MaxThreadSize_) //��ǰ�߳�С���趨���߳�����
		{
			//����һ��Thread����
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int thread_id = ptr->getId();
			thread_list_.emplace(thread_id, std::move(ptr));

			thread_list_[thread_id]->start();
			++spareThreadSize_;
			++currentThreadSize_;
		}
		return result;
	}

private:
	// �����̺߳���
	void threadFunc(int threadid)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();
		for (;;)
		{
			Task task;
			{
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				std::cout << "tid:" << std::this_thread::get_id()
					<< "���Ի�ȡ����..." << std::endl;

				while (TaskQueue_.size() == 0)
				{
					if (!isPoolRunning_)//�̳߳ؽ��� �����߳���Դ
					{
						thread_list_.erase(threadid);
						std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
						exitCond_.notify_all();
						return;//�߳̽���
					}

					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() > THREAD_MAX_IDLE_TIME
								&& currentThreadSize_ > initThreadSize_
								&& currentThreadSize_ > currentTaskSize_)
							{
								thread_list_.erase(threadid);
								std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
								--currentThreadSize_;
								--spareThreadSize_;
								return;//�߳̽���
							}
						}
					}
					else
					{
						notEmpty_.wait(lock);
					}
				}//end while(TaskQueue_.size() == 0)

				--currentThreadSize_;

				task = TaskQueue_.front();
				TaskQueue_.pop();
				--spareThreadSize_;
				std::cout << "tid:" << std::this_thread::get_id()
					<< "��ȡ����ɹ�..." << std::endl;

				//��������û�������еĻ�֪ͨ����߳�
				if (TaskQueue_.size() > 0)
				{
					notEmpty_.notify_all();
				}

				notFull_.notify_all();//֪ͨ�����ύ����
			}

			// ��ǰ�̸߳���ִ���������
			if (task != nullptr)
			{
				// task->run(); // ִ�����񣻰�����ķ���ֵsetVal��������Result
				task();
			}

			++spareThreadSize_;
			lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��

		}//end for (;;)
	}

	// ���pool������״̬
	bool checkRunningState() const
	{
		return isPoolRunning_;
	}
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
	using Task = std::function<void()>;
	std::queue<Task> TaskQueue_;

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