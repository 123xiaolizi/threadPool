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
//启动线程池
void ThreadPool::start(int threadsize)
{
	//设置线程池为启动状态
	isPoolRunning_ = true;
	initThreadSize_ = threadsize;
	currentThreadSize_ = threadsize;
	//创建thread对象
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

//线程执行的函数
void ThreadPool::threadFunc(int threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << "tid:" << std::this_thread::get_id()
				<< "尝试获取任务..." << std::endl;

			while (TaskQueue_.size() == 0)
			{
				if (!isPoolRunning_)//线程池结束 回收线程资源
				{
					thread_list_.erase(threadid);
					std::cout << "threadid:" << std::this_thread::get_id() << " exit!"<< std::endl;
					exitCond_.notify_all();
					return;//线程结束
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
							return;//线程结束
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
				<< "获取任务成功..." << std::endl;

			//看看还有没有任务，有的话通知别的线程
			if (TaskQueue_.size() > 0)
			{
				notEmpty_.notify_all();
			}
			
			notFull_.notify_all();//通知可以提交任务
		}

		// 当前线程负责执行这个任务
		if (task != nullptr)
		{
			// task->run(); // 执行任务；把任务的返回值setVal方法给到Result
			task->exec();
		}

		++spareThreadSize_;
		lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间

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
	//提交任务阻塞时长不超1秒
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return TaskQueue_.size() < MaxTaskSize_;}))
	{
		std::cerr << "task queue is full, submit task fail.---- time_out" << std::endl;
		return Result(sp, false);
	}
	//走到这里说明有任务队列还有空间
	TaskQueue_.emplace(sp);
	currentTaskSize_++;

	//通知工作线程
	notEmpty_.notify_all();

	// cached模式 任务处理比较紧急  判断是否需要创建线程
	if (poolMode_ == PoolMode::MODE_CACHED
		&& spareThreadSize_ < currentTaskSize_	//空闲线程小于任务数
		&& currentThreadSize_ < MaxThreadSize_) //当前线程小于设定的线程上限
	{
		//创建一个Thread对象
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
	// 已经获取的任务的返回值，增加信号量资源
	sem_.post();
}

Any Result::getVal()
{
	//先判断返回值是否可用
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
