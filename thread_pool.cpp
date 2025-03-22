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
//启动线程池
void ThreadPool::start(int threadsize)
{
	//设置线程池为启动状态
	isPoolRunning_ = true;

	spareThreadSize_ = threadsize;
	currentThreadSize_ = threadsize;
	//创建thread对象
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

//线程执行的函数
void ThreadPool::threadFunc(int threadid)
{
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			//上锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			//判断任务队列是否有任务
			std::cout << "线程 " << std::this_thread::get_id() << "正在获取任务。。。" << std::endl;
			notEmpty_.wait(lock);
			
			//获取任务
			task = TaskQueue_.front();
			TaskQueue_.pop();
			--spareThreadSize_;
			--currentTaskSize_;

			std::cout << "tid:" << std::this_thread::get_id()
				<< "获取任务成功..." << std::endl;
			//如果任务队列中还有任务，那就通知其他线程
			if (!TaskQueue_.empty())
			{
				notEmpty_.notify_all();
			}
			//取出任务了，通知任务可以提交了
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

