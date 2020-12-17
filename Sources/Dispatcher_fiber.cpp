//
// Copyright (C) Bercut LLC 1995-2020
// Created by "Aleksey.Dorofeev@bercut.com" on 21.10.2020.
//

#include <queue>
#include <thread>
#include <chrono>
#include <cassert>

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
    assert(g_signal > 0);

    ///tbd: for debug purposes only
    int req_id = 100;
    {
        std::lock_guard lock(g_mt);

        g_mq.push_back(std::make_shared<Incoming_request>(g_rtsib_server, nullptr, ++req_id));
        g_mq.push_back(std::make_shared<Incoming_request>(g_rtsib_server, nullptr, ++req_id));
        g_mq.push_back(std::make_shared<Incoming_request>(g_rtsib_server, nullptr, ++req_id));

        eventfd_write(g_signal, 1ULL);
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

                //TODO: использование файберов в С приводит к непредсказуемым эффектам:
                // 1) результат вызова Lua-функции с асинхронным поведением не в ту функцию С (не тот файбер), которая ждет ее окончания на lua_pcall
                // 2) при параллельном создании файберов в Lua, последний созданный файбер С зависает и не возвращает управления (см. rtsib_test.lua)
                //msg->process();   // для создания файбера-обработчика сообщения из-под C

                //FIXME: создание файберов в Lua решает проблемы
                msg->process_through_lua(); // для создания файбера-обработчика сообщения из-под Lua

                q.pop_front();
            }

            fprintf(stdout, "run_dispatcher_fiber: read >> exit\n");
        }
    }

    say_info("finished");
    return 0;
}
