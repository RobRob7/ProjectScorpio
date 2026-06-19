#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <utility>

template<typename T>
class ThreadSafeQueue
{
public:
	void push(T value)
	{
		{
			std::lock_guard<std::mutex> lock(mtx_);
			queue_.push(std::move(value));
		}
		cv_.notify_one();
	} // end of push()

	bool waitAndPop(T& value)
	{
		std::unique_lock<std::mutex> lock(mtx_);

		cv_.wait(lock, [this] 
			{
				return shutdown_ || !queue_.empty(); 
			});

		if (shutdown_ && queue_.empty())
		{
			return false;
		}

		value = std::move(queue_.front());
		queue_.pop();

		return true;
	} // end of waitAndPop()

	bool tryPop(T& value)
	{
		std::lock_guard<std::mutex> lock(mtx_);

		if (queue_.empty())
		{
			return false;
		}

		value = std::move(queue_.front());
		queue_.pop();

		return true;
	} // end of tryPop()

	void shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(mtx_);
			shutdown_ = true;
		}
		cv_.notify_all();
	} // end of shutdown()

private:
	std::queue<T> queue_;
	std::mutex mtx_;
	std::condition_variable cv_;
	bool shutdown_{ false };
};


#endif
