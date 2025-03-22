#include "thread_pool.h"
#include <iostream>
/////////////////////////Thread//////////////////////////////
Thread::Thread(ThreadFunc func):func_(func), threadID(generaID_++)
{
}
int Thread::generaID_ = 0;

void Thread::start()
{
	
	std::thread t(func_, threadID);
	std::cout << "create thread" << t.get_id() << std::endl;
	t.detach();
}

int Thread::getId() const
{
	return threadID;
}


Thread::~Thread()
{}







//////////////////////////ThreadPool///////////////////////////////////////

ThreadPool::ThreadPool() :
	currentTaskSize_(0),
	spareThreadSize_(0),
	MaxThreadSize_(THREAD_MAX_THRESHHOLD),
	MaxTaskSize_(TASK_MAX_THRESHHOLD),
	initThreadSize_(0),
	currentThreadSize_(0),
	poolMode_(PoolMode::MODE_FIXED),
	isPoolRunning_(false)
{}

ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return thread_list_.size() == 0; });
}
//�����̳߳�
void ThreadPool::start(int threadsize)
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

//�߳�ִ�еĺ���
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid:" << std::this_thread::get_id()
				<< "���Ի�ȡ����..." << std::endl;

			while (TaskQueue_.size() == 0)
			{
				if (!isPoolRunning_)//�̳߳ؽ��� �����߳���Դ
				{
					thread_list_.erase(threadid);
					std::cout << "threadid:" << std::this_thread::get_id() << " exit!"<< std::endl;
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
			task->exec();
		}

		++spareThreadSize_;
		lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��

	}//end for (;;)
}

void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	MaxTaskSize_ = threshhold;
}

void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	MaxThreadSize_ = threshhold;
}

Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//�ύ��������ʱ������1��
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return TaskQueue_.size() < MaxTaskSize_;}))
	{
		std::cerr << "task queue is full, submit task fail.---- time_out" << std::endl;
		return Result(sp, false);
	}
	//�ߵ�����˵����������л��пռ�
	TaskQueue_.emplace(sp);
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
	return Result(sp);
}



bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}


/////////////////Result
Result::Result(std::shared_ptr<Task> task, bool isValid):
	isValid_(isValid),task_(task)
{
	task_->setResult(this);
}

void Result::setVal(Any any)
{
	this->any_ = std::move(any);
	// �Ѿ���ȡ������ķ���ֵ�������ź�����Դ
	sem_.post();
}

Any Result::getVal()
{
	//���жϷ���ֵ�Ƿ����
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();
	return std::move(any_);
}

///////////////Task
Task::Task() :result_(nullptr)
{}


void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());
	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}
