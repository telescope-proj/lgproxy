#ifndef LP_QUEUE_H_
#define LP_QUEUE_H_

#include <deque>
#include <stdint.h>

namespace lp {

template <typename T, uint64_t max_size>
class fixed_deque : public std::deque<T> {

public:

    void push(const T& value) {
        if (this->size() == max_size) {
            lp_log_trace("Queue overflow; dropping oldest item");
            this->pop_front();
        }
        std::deque<T>::push_back(value);
    }

};

};

#endif