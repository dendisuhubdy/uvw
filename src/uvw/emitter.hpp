#pragma once


#include <type_traits>
#include <functional>
#include <algorithm>
#include <utility>
#include <cstddef>
#include <vector>
#include <memory>
#include <list>
#include "event.hpp"


namespace uvw {


/**
 * @brief Event emitter base class.
 *
 * Almost everything in `uvw` is an event emitter.<br/>
 * This is the base class from which resources and loops inherit.
 */
template<typename T>
class Emitter {
    struct BaseHandler {
        virtual ~BaseHandler() noexcept = default;
        virtual bool empty() const noexcept = 0;
        virtual void clear() noexcept = 0;
    };

    template<typename E>
    struct Handler final: BaseHandler {
        using Listener = std::function<void(E &, T &)>;
        using Element = std::pair<bool, Listener>;
        using ListenerList = std::list<Element>;
        using Connection = typename ListenerList::iterator;

        bool empty() const noexcept override {
            auto pred = [](auto &&element){ return element.first; };

            return std::all_of(onceL.cbegin(), onceL.cend(), pred) &&
                    std::all_of(onL.cbegin(), onL.cend(), pred);
        }

        void clear() noexcept override {
            auto func = [](auto &&element){ element.first = true; };
            std::for_each(onceL.begin(), onceL.end(), func);
            std::for_each(onL.begin(), onL.end(), func);
        }

        Connection once(Listener f) {
            return onceL.emplace(onceL.cend(), false, std::move(f));
        }

        Connection on(Listener f) {
            return onL.emplace(onL.cbegin(), false, std::move(f));
        }

        void erase(Connection conn) noexcept {
            conn->first = true;
        }

        void publish(E event, T &ref) {
            ListenerList currentL;
            onceL.swap(currentL);

            auto func = [&event, &ref](auto &&element) {
                return element.first ? void() : element.second(event, ref);
            };

            std::for_each(onL.rbegin(), onL.rend(), func);
            std::for_each(currentL.rbegin(), currentL.rend(), func);

            onL.remove_if([](auto &&element){ return element.first; });
        }

    private:
        ListenerList onceL{};
        ListenerList onL{};
    };

    template<typename E>
    Handler<E> & handler() noexcept {
        static_assert(std::is_base_of<Event<E>, E>::value, "!");

        std::size_t type = E::type();

        if(!(type < handlers.size())) {
            handlers.resize(type+1);
        }

        if(!handlers[type]) {
           handlers[type] = std::make_unique<Handler<E>>();
        }

        return static_cast<Handler<E>&>(*handlers[type]);
    }

protected:
    template<typename E>
    void publish(E event) {
        handler<E>().publish(std::move(event), *static_cast<T*>(this));
    }

public:
    template<typename E>
    using Listener = typename Handler<E>::Listener;

    /**
     * @brief Connection type for a given event type.
     *
     * Given an event type `E`, `Connection<E>` is the type of the connection
     * object returned by the event emitter whenever a listener for the given
     * type is registered.
     */
    template<typename E>
    struct Connection: private Handler<E>::Connection {
        template<typename> friend class Emitter;

        Connection() = default;
        Connection(const Connection &) = default;
        Connection(Connection &&) = default;

        Connection(typename Handler<E>::Connection conn)
            : Handler<E>::Connection{std::move(conn)}
        { }

        Connection & operator=(const Connection &) = default;
        Connection & operator=(Connection &&) = default;
    };

    virtual ~Emitter() noexcept {
        static_assert(std::is_base_of<Emitter<T>, T>::value, "!");
    }

    /**
     * @brief Registers a long-lived listener with the event emitter.
     *
     * This method can be used to register a listener that is meant to be
     * invoked more than once for the given event type.<br/>
     * The Connection object returned by the method can be freely discarded. It
     * can be used later to disconnect the listener, if needed.
     *
     * Listener is usually defined as a callable object assignable to a
     * `std::function<void(const E &, T &)`, where `E` is the type of the event
     * and `T` is the type of the resource.
     *
     * @param f A valid listener to be registered.
     * @return Connection object to be used later to disconnect the listener.
     */
    template<typename E>
    Connection<E> on(Listener<E> f) {
        return handler<E>().on(std::move(f));
    }

    /**
     * @brief Registers a short-lived listener with the event emitter.
     *
     * This method can be used to register a listener that is meant to be
     * invoked only once for the given event type.<br/>
     * The Connection object returned by the method can be freely discarded. It
     * can be used later to disconnect the listener, if needed.
     *
     * Listener is usually defined as a callable object assignable to a
     * `std::function<void(const E &, T &)`, where `E` is the type of the event
     * and `T` is the type of the resource.
     *
     * @param f Avalid listener to be registered.
     * @return Connection object to be used later to disconnect the listener.
     */
    template<typename E>
    Connection<E> once(Listener<E> f) {
        return handler<E>().once(std::move(f));
    }

    /**
     * @brief Disconnects a listener from the event emitter.
     * @param conn A valid Connection object
     */
    template<typename E>
    void erase(Connection<E> conn) noexcept {
        handler<E>().erase(std::move(conn));
    }

    /**
     * @brief Disconnects all the listeners for the given event type.
     */
    template<typename E>
    void clear() noexcept {
        handler<E>().clear();
    }

    /**
     * @brief Disconnects all the listeners.
     */
    void clearAll() noexcept {
        std::for_each(handlers.begin(), handlers.end(),
                      [](auto &&handler){ if(handler) { handler->clear(); } });
    }

    /**
     * @brief Checks if there are listeners registered for the specific event.
     * @return True if there are no listeners registered for the specific event,
     * false otherwise.
     */
    template<typename E>
    bool empty() const noexcept {
        std::size_t type = E::type();

        return (!(type < handlers.size()) ||
                !handlers[type] ||
                static_cast<Handler<E>&>(*handlers[type]).empty());
    }

    /**
     * @brief Checks if there are listeners registered with the event emitter.
     * @return True if there are no listeners registered with the event emitter,
     * false otherwise.
     */
    bool empty() const noexcept {
        return std::all_of(handlers.cbegin(), handlers.cend(),
                           [](auto &&handler){ return !handler || handler->empty(); });
    }

private:
    std::vector<std::unique_ptr<BaseHandler>> handlers{};
};


}
