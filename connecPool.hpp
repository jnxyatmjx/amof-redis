#ifndef H_connecPool_H
#define H_connecPool_H

#include <deque>
#include <memory>
#include <functional>

#include <condition_variable>
#include <mutex>
#include <thread>

template <class T>
class connecPool final
{
public:

	using DeleterType = std::function<void(T*)>;
	//typedef std::function<void(T*)> DeleterType;

	explicit connecPool(std::size_t maxsize) :totalsize_(maxsize), cursize_(0), reset_cluster(false){}
	~connecPool(){}

	void initialize()
	{
		for (size_t i = 0; i < totalsize_; i++)
		{
			std::unique_ptr<T> ts_(new T());
			pool_.push_back(std::move(ts_));
			cursize_++;
		}

	}

	std::size_t size()
	{
		std::lock_guard<std::mutex> lock(lc_);
		return pool_.size();
	}

	bool empty()
	{
		std::lock_guard<std::mutex> lock(lc_);
		return (cursize_ == 0) || (reset_cluster == true);
	}

	//return maybe NULL
	std::unique_ptr<T,DeleterType> acquire()
	{
		std::unique_lock<std::mutex> lock(lc_);

		//destroying the pool, do not acquire any things
		if (reset_cluster) return NULL;

		while (pool_.empty())
			cond_.wait(lock);

		//std::unique_ptr<T> ptr_ = std::move(pool_.front());
		//pool_.pop_front();

		std::unique_ptr<T, DeleterType> ptr_(pool_.front().release(), [this](T* const t) {
			
			std::lock_guard<std::mutex> loskc(lc_);

			if (reset_cluster == false)
			{
				pool_.push_back(std::unique_ptr<T>(t));
			}
			else //when cluster master info updated , the used connection should destroy itself
			{
				delete t;
			}

			cursize_++;
			cond_.notify_one();
		});

		cursize_--;
		pool_.pop_front();

		return std::move(ptr_);
	}

	std::vector< std::unique_ptr<T, DeleterType> > acquireIdls()
	{
		std::vector< std::unique_ptr<T, DeleterType> > res_vec;
		std::unique_lock<std::mutex> lock(lc_);

		//destroying the pool, do not acquire any things
		if (reset_cluster) return std::move(res_vec);

		while (!pool_.empty())
		{
			std::unique_ptr<T, DeleterType> ptr_(pool_.front().release(), [this](T* const t) {

				std::lock_guard<std::mutex> loskc(lc_);

				if (reset_cluster == false)
				{
					pool_.push_back(std::unique_ptr<T>(t));
				}
				else //when cluster master info updated , the used connection should destroy itself
				{
					delete t;
				}
				cursize_++;
				cond_.notify_one();
			});

			cursize_--;
			pool_.pop_front();

			res_vec.push_back(std::move(ptr_));
		}
		return std::move(res_vec);
	}

	bool destroy()
	{
		std::unique_lock<std::mutex> loskc(lc_);
		reset_cluster = true;
		
		//the idle object
		for (auto& obj_ : pool_)
		{
			obj_.reset();
		}
		pool_.clear();

		//the used object
		while (cursize_ < totalsize_)
			cond_.wait(loskc);

		return cursize_ == totalsize_ ;
	}

private:
	std::size_t totalsize_;
	std::size_t cursize_;
	bool reset_cluster;
	std::deque<std::unique_ptr<T> > pool_;
	std::mutex lc_;
	std::condition_variable cond_;
};

#endif