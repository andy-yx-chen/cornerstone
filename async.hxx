#ifndef _ASYNC_HXX_
#define _ASYNC_HXX_

namespace cornerstone {
	template<typename T>
	class async_result {
	public:
		typedef std::function<void(T, std::exception*)> handler_type;
		async_result() : has_result_(false), err_(nilptr), lock_(), cv_() {}
		async_result(T result)
			: result_(result), has_result_(true), err_(nilptr), lock_(), cv_() {}

		~async_result() {}

		__nocopy__(async_result)

	public:
		void when_ready(handler_type& handler) {
			std::lock_guard<std::mutex> guard(lock_);
			if (has_result_) {
				handler(result_, err_);
			}
			else {
				handler_ = handler;
			}
		}

		void set_result(T result, std::exception* err) {
			{
				std::lock_guard<std::mutex> guard(lock_);
				result_ = result;
				err_ = err;
				has_result_ = true;
			}

			cv_.notify_all();
			if (handler_) {
				handler_(result, err);
			}
		}

		T get() {
			std::unique_lock<std::mutex> lock(lock_);
			if (has_result_) {
				if (err_ == nullptr) {
					return result_;
				}

				throw err_;
			}

			cv_.wait(lock);
			if (err_ == nullptr) {
				return result_;
			}

			throw err_;
		}

	private:
		T result_;
		std::exception* err_;
		bool has_result_;
		handler_type handler_;
		std::mutex lock_;
		std::condition_variable cv_;
	};
}

#endif //_ASYNC_HXX_
