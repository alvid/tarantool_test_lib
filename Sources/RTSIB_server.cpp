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

int create_server(lua_State *lua)
{
    std::string port_type(luaL_checkstring(lua, 1));
    std::string callback(luaL_checkstring(lua, 2));
    say_info("luartsib: create_server('%s', '%s')", port_type.c_str(), callback.c_str());
    //for debug purposes only
    if (g_rtsib_server == nullptr) {
        g_rtsib_server = new RTSIB_Server(port_type, callback);
    }
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

    auto L = luaT_state();

    printf("RTSIB_Server::on_incoming_request(event = %p): >> call %s()\n",
           cur_event.get(), lua_callback.c_str());
    lua_getglobal(L, lua_callback.c_str());
    lua_pushnumber(L, cur_event->request_id);
    auto rc = lua_pcall(L, 1, 1, 0);
    if (rc) {
        fprintf(stderr, "error running function '%s': %s\n", lua_callback.c_str(), lua_tostring(L, -1));
        return -1;
        //error(L, "error running function '%s': %s", lua_callback.c_str(), lua_tostring(L, -1));
    }
    auto response_id = lua_tonumber(L, -1);
    lua_pop(L, 1);
    printf("RTSIB_Server::on_incoming_request(event = %p): << call %s() = %d\n",
            cur_event.get(), lua_callback.c_str(), response_id);

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
    auto f = fiber_new("rtsib_server_callback", rtsib_server_callback_f);
    if (f) {
        fiber_start(f, rtsib_server);
    }
}

Incoming_request::~Incoming_request()
{
    printf("Incoming_request::dtor(%d) = %p\n", request_id, this);
}
