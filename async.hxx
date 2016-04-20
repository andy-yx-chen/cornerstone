#ifndef _ASYNC_HXX_
#define _ASYNC_HXX_

namespace cornerstone {
	template<typename T>
	class async_result {
	public:
		async_result() : has_result_(false), err_(nilptr) {}
		async_result(T result) 
			: result_(result), has_result_(true), err_(nilptr){}

		~async_result() {
			if (err_ != null) {
				delete err_;
			}
		}

		__nocopy__(async_result)

	public:
		void when_ready(std::function<void(T, std::exception*) handler) {
			if (has_result_) {
				handler(result_, err_);
			}
		}

	private:
		T result_;
		std::exception* err_;
		bool has_result_;
		std::function<void(T, std::exception*)> handler_;
	};
}

#endif //_ASYNC_HXX_
