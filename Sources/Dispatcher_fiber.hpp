//
// Copyright (C) Bercut LLC 1995-2020
// Created by "Aleksey.Dorofeev@bercut.com" on 21.10.2020.
//

#ifndef LUA_RTSIB_DISPATCHER_FIBER_HPP
#define LUA_RTSIB_DISPATCHER_FIBER_HPP

#include <sys/eventfd.h>
#include <memory>
#include <deque>
#include <mutex>

struct RTSIB_event {
    virtual ~RTSIB_event() {}
    virtual void process() = 0;
    virtual void process_through_lua() { process(); }
};

extern std::mutex g_mt;
extern std::deque<std::shared_ptr<RTSIB_event>> g_mq;
extern int g_signal;

int run_dispatcher_fiber(lua_State *lua);

#endif //LUA_RTSIB_DISPATCHER_FIBER_HPP
