#pragma once

namespace lr {
template<typename EventT, typename DataT>
struct EventManager {
    struct EventMetadata {
        EventT event = EventT::Invalid;
        DataT data = {};
    };
    plf::colony<EventMetadata> queue = {};

    bool peek(this auto &self) { return !self.queue.empty(); }

    EventT dispatch(this auto &self, DataT &data_out)
    {
        ZoneScoped;

        if (!self.queue.empty()) {
            auto begin_it = self.queue.begin();
            EventMetadata &v = *begin_it;
            EventT event_type = v.event;
            data_out = v.data;

            self.queue.erase(begin_it);

            return event_type;
        }

        return EventT::Invalid;
    }

    void push(this auto &self, EventT event, const DataT &data)
    {
        ZoneScoped;

        self.queue.emplace(event, data);
    }
};

}  // namespace lr
