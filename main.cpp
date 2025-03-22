#include <iostream>
#include <chrono>
#include <thread>
#include "thread_pool.h"
using namespace std;




using uLong = unsigned long long;

class MyTask : public Task
{
public:
    MyTask(int begin, int end)
        : begin_(begin)
        , end_(end)
    {}
    
    Any run()  // run方法最终就在线程池分配的线程中去做执行了!
    {
        std::cout << "tid:" << std::this_thread::get_id()
            << "begin!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        uLong sum = 0;
        for (uLong i = begin_; i <= end_; i++)
            sum += i;
        std::cout << "tid:" << std::this_thread::get_id()
            << "end!" << std::endl;

        return sum;
    }

private:
    int begin_;
    int end_;
};

int main()
{
    {
        ThreadPool pool;
        //pool.setMode(PoolMode::MODE_CACHED);
        // 开始启动线程池
        pool.start(4);

        // linux上，这些Result对象也是局部对象，要析构的！！！
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        //pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        //pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        //pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        //pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));

        uLong sum1 = res1.getVal().cast_<uLong>();
        uLong sum2 = res2.getVal().cast_<uLong>();
        uLong sum3 = res3.getVal().cast_<uLong>();
        uLong sum4 = sum1 + sum2;
        sum4 += sum3;

        cout << sum4  << endl;
    } // 这里Result对象也要析构!!! 在vs下，条件变量析构会释放相应资源的
    uLong sum = 0;
    for (int i = 1; i <= 300000000; ++i)
    {
        sum += i;
    }
    cout << sum << endl;
    cout << "main over!" << endl;
    getchar();

}