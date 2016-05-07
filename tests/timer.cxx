#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <iostream>
#include <functional>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <unistd.h>

class task{
public:
    task(std::function<void(void*)>& func, void* ctx)
	: func_(func), ctx_(ctx), lock_(), cancelled_(false), handle_(nullptr){}

    ~task(){
	std::cout << "task deleted" << std::endl;
    }
    
private:
    task(const task&) = delete;
    task& operator = (const task&) = delete;

public:
    void execute() {
	std::lock_guard<std::mutex> guard(lock_);
	if(!cancelled_){
	    func_(ctx_);
	}
    }

    void cancel(){
	std::lock_guard<std::mutex> guard(lock_);
	cancelled_ = true;
    }

    void set_handle(void* handle){
	handle_ = handle;
    }

    void* get_handle(){
	return handle_;
    }

private:
    std::function<void(void*)> func_;
    void* ctx_;
    std::mutex lock_;
    bool cancelled_;
    void* handle_;
};

class task_scheduler{
public:
    task_scheduler(){
	kq_ = kqueue();
	std::thread t(std::bind(&task_scheduler::poll, this));
	t.detach();
    }

    ~task_scheduler(){
	if(kq_ != -1){
	    close(kq_);
	}
    }

private:
    task_scheduler(const task_scheduler&) = delete;
    task_scheduler& operator=(const task_scheduler&) = delete;

public:
    void schedule(std::shared_ptr<task>& t, int milliseconds);
    void cancel(std::shared_ptr<task>& t);
private:
    void poll();
private:
    int kq_;
};

void task_scheduler::schedule(std::shared_ptr<task>& t, int milliseconds){
    if(kq_ == -1){
	throw std::runtime_error("bad timer queue");
    }

    struct kevent change;
    std::shared_ptr<task>* ident = new std::shared_ptr<task>(t);
    t->set_handle(ident);
    EV_SET(&change, (uintptr_t)ident, EVFILT_TIMER, EV_ADD|EV_ENABLE|EV_ONESHOT, 0, milliseconds, 0);
    int nev = kevent(kq_, &change, 1, nullptr, 0, nullptr);
    if(nev < 0){
	throw std::runtime_error("failed to queue the task");
    }
}

void task_scheduler::poll(){
    struct kevent events[10];
    for(;;){
	int n = kevent(kq_, nullptr, 0, events, 10, nullptr);
	if(n < 0){
	    std::cout << "error! failed to poll events from queue" << std::endl;
	    return;
	}

	for(int i = 0; i < n; ++i){
	    std::shared_ptr<task>* t = (std::shared_ptr<task>*)events[i].ident;
	    if(!(events[i].flags & EV_ERROR)){
		(*t)->execute();
	    }
	    else{
		std::cout << "event error!" << std::endl;
	    }

	    delete t;
	}
    }
}

void task_scheduler::cancel(std::shared_ptr<task>& t) {
    struct kevent change;
    EV_SET(&change, (uintptr_t)t->get_handle(), EVFILT_TIMER, EV_DELETE, 0, 0, 0);
    int nev = kevent(kq_, &change, 1, nullptr, 0, nullptr);
    t->cancel();
    delete (std::shared_ptr<task>*)t->get_handle();
    if(nev < 0){
	throw std::runtime_error("failed to cancel the task");
    }
}

int main(){
    task_scheduler scheduler;
    std::function<void(void*)> func = (std::function<void(void*)>)([=](void* c) -> void {
	    std::cout << "timer fired!" << std::endl;
	});
    std::shared_ptr<task> t(new task(func, 0));
    scheduler.schedule(t, 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    scheduler.schedule(t, 2000);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    scheduler.cancel(t);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << "exiting..." << std::endl;
    return 0;
}
