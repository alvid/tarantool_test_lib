//
// Copyright (C) Bercut LLC 1995-2020
// Created by "Aleksey.Dorofeev@bercut.com" on 21.10.2020
//

#include <cassert>
#include <pthread.h>
#include "lua.hpp"
#include "RTSIB_server.hpp"
#include "Dispatcher_fiber.hpp"

RTSIB_Server *g_rtsib_server = nullptr;

int create_server(lua_State *state)
{
    std::string port_type(luaL_checkstring(state, 1));
    std::string callback(luaL_checkstring(state, 2));
    say_info("luartsib: create_server('%s', '%s')", port_type.c_str(), callback.c_str());
    //for debug purposes only
    if (g_rtsib_server == nullptr) {
        g_rtsib_server = new RTSIB_Server(port_type, callback);
    }
    return 0;
}

int run_request_fiber(lua_State *state)
{
    int request_id = luaL_checkinteger(state, 1);
    printf("run_request_fiber_%d: run", request_id);
    g_rtsib_server->on_incoming_request(state, request_id);
    printf("run_request_fiber_%d: end", request_id);
    return 0;
}

RTSIB_Server::RTSIB_Server(std::string a_port_type, std::string cb)
    : port_type(std::move(a_port_type))
    , lua_callback(cb)
{
    snprintf(log_prefix, sizeof(log_prefix), "luartsib: server('%s'): ", port_type.c_str());
    say_info("%s%s", log_prefix, "created");
}

RTSIB_Server::~RTSIB_Server()
{
    say_info("%s%s", log_prefix, "destroyed");
}

void RTSIB_Server::on_request(void *msg, const unsigned int req_id)
{
    fprintf(stdout, "(thread 0x%x): RTSIB_Server::on_request(): << enter\n", pthread_self());

    // разрыв потока исполнения, за которым должен последовать вызов rtsib_server_callback из диспетчерского файбера
    {
        std::lock_guard lock(g_mt);

        auto isNeedSignal = g_mq.empty();
        g_mq.push_back(std::make_shared<Incoming_request>(this, msg, req_id));
        if (isNeedSignal)
            eventfd_write(g_signal, 1ULL);
    }

    fprintf(stdout, "(thread 0x%x): RTSIB_Server::on_request(): >> exit\n", pthread_self());
}

void RTSIB_Server::set_context_event(std::shared_ptr<Incoming_request> const& ev)
{
    fc.event = ev;
}

int RTSIB_Server::on_incoming_request()
{
    std::shared_ptr<Incoming_request> cur_event;
    cur_event.swap(fc.event);

    auto state = luaT_state();

    lua_getglobal(state, lua_callback.c_str());
    lua_pushnumber(state, cur_event->request_id);
    printf("RTSIB_Server::on_incoming_request(event = %p): >> call %s(%d)\n",
           cur_event.get(), lua_callback.c_str(), cur_event->request_id);
    auto rc = luaT_call(state, 1, 1); // lua_pcall(L, 1, 1, 0);
    if (rc) {
        fprintf(stderr, "error running function '%s': %s\n", lua_callback.c_str(), lua_tostring(state, -1));
        lua_pop(state, 1);
        return -1;
        //return luaT_error(state);
    }
    assert(lua_isnumber(state, -1));
    auto response_id = lua_tointeger(state, -1);
    printf("RTSIB_Server::on_incoming_request(event = %p): << %s() return %d, wait %d\n",
           cur_event.get(), lua_callback.c_str(), response_id, cur_event->request_id);
    lua_pop(state, 1);

    return 0;
}

int RTSIB_Server::on_incoming_request(lua_State *state, int request_id)
{
    lua_getglobal(state, lua_callback.c_str());
    lua_pushnumber(state, request_id);
    printf("RTSIB_Server::on_incoming_request(): >> call %s(%d)\n",
           lua_callback.c_str(), request_id);
    auto rc = lua_pcall(state, 1, 1, 0);
    if (rc) {
        fprintf(stderr, "error running function '%s': %s\n", lua_callback.c_str(), lua_tostring(state, -1));
        lua_pop(state, 1);
        return -1;
    }
    assert(lua_isnumber(state, -1));
    auto response_id = lua_tointeger(state, -1);
    printf("RTSIB_Server::on_incoming_request(): << %s() return %d, wait %d\n",
           lua_callback.c_str(), response_id, request_id);
    lua_pop(state, 1);

    return 0;
}

static int rtsib_server_callback_f(va_list vl)
{
    RTSIB_Server* rtsib_server = va_arg(vl, RTSIB_Server*);
    rtsib_server->on_incoming_request();
    return 0;
}

Incoming_request::Incoming_request(RTSIB_Server *a_server, void *a_msg, unsigned int a_request_id)
    : rtsib_server(a_server)
    , msg(a_msg)
    , request_id(a_request_id)
{
    printf("Incoming_request::ctor(%d) = %p\n", request_id, this);
}

void Incoming_request::process()
{
    rtsib_server->set_context_event(shared_from_this());
    std::string name = "rtsib_server_callback_" + std::to_string(request_id);
    _fiber = fiber_new(name.c_str(), rtsib_server_callback_f);
    if (_fiber) {
        fiber_start(_fiber, rtsib_server);
    }
}

Incoming_request::~Incoming_request()
{
    printf("Incoming_request::dtor(%d) = %p\n", request_id, this);
}

void Incoming_request::process_through_lua()
{
    auto state = luaT_state();

    std::string lua_callback = "onRequestToRunFiber";

    lua_getglobal(state, lua_callback.c_str());
    lua_pushinteger(state, request_id);
    printf("Incoming_request::process_through_lua(event = %p): >> call %s(%d)\n",
           this, lua_callback.c_str(), request_id);
    auto rc = lua_pcall(state, 1, 0, 0);
    if (rc) {
        fprintf(stderr, "error running function '%s': %s\n",
                lua_callback.c_str(), lua_tostring(state, -1));
        lua_pop(state, 1);
        return;
    }
    printf("Incoming_request::process_through_lua(event = %p): << %s()\n",
           this, lua_callback.c_str());
}
