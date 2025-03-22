#include "thread_pool.h"
#include <iostream>
/////////////////////////Thread//////////////////////////////
Thread::Thread(ThreadFunc func):func_(func), threadID(generaID_++)
{
}

void Thread::start()
{
	std::thread t(func_, threadID);
	t.detach();
}

int Thread::getId() const
{
	return threadID;
}








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
//�����̳߳�
void ThreadPool::start(int threadsize)
{
	//�����̳߳�Ϊ����״̬
	isPoolRunning_ = true;

	spareThreadSize_ = threadsize;
	currentThreadSize_ = threadsize;
	//����thread����
	for (int i = 0; i < threadsize && i < MaxThreadSize_; ++i)
	{
		auto thread_ptr = std::make_unique<Thread>(std::bind(ThreadPool::threadFunc, this, std::placeholders::_1));
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
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			//����
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			//�ж���������Ƿ�������
			std::cout << "�߳� " << std::this_thread::get_id() << "���ڻ�ȡ���񡣡���" << std::endl;
			notEmpty_.wait(lock);
			
			//��ȡ����
			task = TaskQueue_.front();
			TaskQueue_.pop();
			--spareThreadSize_;
			--currentTaskSize_;

			std::cout << "tid:" << std::this_thread::get_id()
				<< "��ȡ����ɹ�..." << std::endl;
			//�����������л��������Ǿ�֪ͨ�����߳�
			if (!TaskQueue_.empty())
			{
				notEmpty_.notify_all();
			}
			//ȡ�������ˣ�֪ͨ��������ύ��
			notFull_.notify_all();
		}
		task->start();

	}
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

void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notFull_.wait(lock);
	TaskQueue_.emplace(std::move(sp));
	++currentTaskSize_;
}



bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

