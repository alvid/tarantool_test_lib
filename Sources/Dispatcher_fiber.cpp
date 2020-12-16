//
// Copyright (C) Bercut LLC 1995-2020
// Created by "Aleksey.Dorofeev@bercut.com" on 21.10.2020.
//

#include <queue>
#include <cstdlib>
#include <thread>
#include <chrono>

#include "lua.hpp"
#include "tarantool/module.h"
#include "Dispatcher_fiber.hpp"
#include "RTSIB_server.hpp"

std::mutex g_mt;
std::deque<std::shared_ptr<RTSIB_event>> g_mq;
int g_signal = -1;

#define START_REQUEST_ID 1000
#define REQUEST_COUNT 1000U

static void thread_proc(int start_request_id, size_t request_count)
{
    for (size_t i = 0; i < request_count; ++i) {
//        std::this_thread::sleep_for(std::chrono::milliseconds(1));
//        g_rtsib_server->on_request(nullptr, ++start_request_id);
//        g_rtsib_server->on_request(nullptr, ++start_request_id);
        g_rtsib_server->on_request(nullptr, ++start_request_id);
    }
}

int run_dispatcher_fiber(lua_State *lua)
{
    say_info("started");

    g_signal = eventfd(0, 0);
    if (g_signal < 0) {
        throw std::runtime_error(std::string("Error happens while call eventfd() - ")
            .append(strerror(errno)));
    }

    ///tbd: for debug purposes only
    int req_id = 100;
    {
        auto m1 = std::make_shared<Incoming_request>(g_rtsib_server, nullptr, ++req_id);
        auto m2 = std::make_shared<Incoming_request>(g_rtsib_server, nullptr, ++req_id);
        auto m3 = std::make_shared<Incoming_request>(g_rtsib_server, nullptr, ++req_id);

        {
            std::lock_guard lock(g_mt);

            g_mq.push_back(m1);
            g_mq.push_back(m2);
            g_mq.push_back(m3);

            eventfd_write(g_signal, 1ULL);
        }
    }
    ///tbd: for debug purposes only

    //std::thread t(thread_proc, START_REQUEST_ID, REQUEST_COUNT);

    while (true) {
        int rc = coio_wait(g_signal, COIO_READ, 1);

        if (fiber_is_cancelled())
            break;

        if (rc & COIO_READ) {
            fprintf(stdout, "run_dispatcher_fiber: read << enter\n");

            eventfd_t v;
            eventfd_read(g_signal, &v);

            std::deque<std::shared_ptr<RTSIB_event>> q;

            g_mt.lock();
            while (!g_mq.empty()) {
                auto msg = g_mq.front();
                q.push_back(msg);
                g_mq.pop_front();
            }
            g_mt.unlock();

            fprintf(stdout, "run_dispatcher_fiber: read %d message(s)\n", q.size());

            while (!q.empty()) {
                auto msg = q.front();
                msg->process();
                q.pop_front();
            }

            fprintf(stdout, "run_dispatcher_fiber: read >> exit\n");
        }
    }

    say_info("finished");
    return 0;
}
