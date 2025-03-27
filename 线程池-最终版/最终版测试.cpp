#include <iostream>
#include "thread_pool_×îÖÕ°æ.h"
#include <chrono>
#include <thread>
int sum(int a, int b)
{
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return a + b;
}
int sum2(int a)
{
	std::this_thread::sleep_for(std::chrono::seconds(2));
	int sum = 0;
	for (int i = 1; i <= a; ++i)
	{
		sum += i;
	}
	return sum;
}
int main()
{
	{
		ThreadPool pool;
		//pool.setMode(PoolMode::MODE_CACHED);
		pool.start(2);
		std::future<int> res1 = pool.submitTask(sum, 2, 3);
		std::future<int> res2 = pool.submitTask(sum2, 100);
		std::future<int> res3 = pool.submitTask(sum, 1, 2);
		std::future<int> res4 = pool.submitTask(sum2, 100);
		std::future<int> res5 = pool.submitTask(sum2, 100);
		std::future<int> res6 = pool.submitTask(sum2, 100);
		std::future<int> res7 = pool.submitTask(sum, 2, 3);

		std::cout << res1.get() << std::endl;
		std::cout << res2.get() << std::endl;
		std::cout << res3.get() << std::endl;
		std::cout << res4.get() << std::endl;
		std::cout << res5.get() << std::endl;
		std::cout << res6.get() << std::endl;
		std::cout << res7.get() << std::endl;


	}

	return 0;
}