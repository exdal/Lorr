#pragma once

#include <deque>

#define LR_INVALID_EVENT ~0u

namespace lr {
using Event = u32;
template<typename DataT>
struct EventManager {
    struct EventMetadata {
        Event event = {};
        DataT data = {};
    };

    bool peek() { return !m_queue.empty(); }

    Event dispatch(DataT &data_out)
    {
        ZoneScoped;

        if (!m_queue.empty()) {
            EventMetadata &v = m_queue.front();
            data_out = v.data;

            m_queue.pop_front();

            return v.event;
        }

        return LR_INVALID_EVENT;
    }

    void push(Event event, DataT &data)
    {
        ZoneScoped;

        m_queue.emplace_back(event, data);
    }

    std::deque<EventMetadata> m_queue = {};
};

}  // namespace lr
