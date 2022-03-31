#pragma once

#include <railguard/utils/storage.h>

#include <functional>

namespace rg
{
    template<typename EventData = std::nullptr_t>
    class EventSender
    {
      public:
        // Types
        using Id                    = typename Storage<EventData>::Id;
        constexpr static Id NULL_ID = Storage<EventData>::NULL_ID;

        typedef std::function<void(const EventData &event_data)> EventHandler;

      private:
        Storage<EventHandler> m_subscribers;

      public:
        Id subscribe(const EventHandler &event_handler)
        {
            return m_subscribers.push(event_handler);
        }

        Id subscribe(EventHandler &&event_handler)
        {
            return m_subscribers.push(std::move(event_handler));
        }

        void unsubscribe(Id id)
        {
            m_subscribers.remove(id);
        }

        void send(EventData &event_data)
        {
            for (auto &entry : m_subscribers)
            {
                auto &handler = entry.value();
                handler(event_data);
            }
        }

        void send(EventData &&event_data)
        {
            for (auto &entry : m_subscribers)
            {
                auto &handler = entry.value();
                handler(event_data);
            }
        }
    };
} // namespace rg