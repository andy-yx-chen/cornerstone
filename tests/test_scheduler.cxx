#include "../cornerstone.hxx"
#include <iostream>
#include <cassert>

using namespace cornerstone;
int _timer_fired_counter = 0;
void test_scheduler() {
    asio_service svc;
    timer_task<void>::executor handler = (std::function<void()>)([]() -> void {
        _timer_fired_counter++;
    });
    std::shared_ptr<delayed_task> task(new timer_task<void>(handler));
    std::cout << "scheduled to wait for 200ms" << std::endl;
    svc.schedule(task, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    assert(_timer_fired_counter == 1);

    std::cout << "scheduled to wait for 300ms" << std::endl;
    svc.schedule(task, 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "cancel the timer task" << std::endl;
    svc.cancel(task);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    assert(_timer_fired_counter == 1);

    std::cout << "scheduled to wait for 300ms" << std::endl;
    svc.schedule(task, 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    assert(_timer_fired_counter == 2);
    svc.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}