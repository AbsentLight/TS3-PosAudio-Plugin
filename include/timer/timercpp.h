#include <iostream>
#include <thread>
#include <chrono>

//https://github.com/99xt/timercpp
//https://www.fluentcpp.com/2018/12/28/timer-cpp/

class Timer {
    bool clear = false;

    public:
        void setTimeout(void (*function)(uint64), uint64 id, int delay);
        void setInterval(void (*function)(uint64), uint64 id, int interval);
        void stop();

};

void Timer::setTimeout(void (*function)(uint64), uint64 id, int delay) {
    std::thread t([=]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        function(id);
    });
    t.detach();
}

void Timer::setInterval(void (*function)(uint64), uint64 id, int interval) {
    this->clear = false;
    std::thread t([=]() {
        while(true) {
            if(this->clear) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            if(this->clear) return;
            function(id);
        }
    });
    t.detach();
}

void Timer::stop() {
    this->clear = true;
}