//
// Copyright (C) Bercut LLC 1995-2020
// Created by "Aleksey.Dorofeev@bercut.com" on 21.10.2020
//

#ifndef LUA_RTSIB_RTSIB_SERVER_HPP
#define LUA_RTSIB_RTSIB_SERVER_HPP

#include <string>
#include <functional>

#include "tarantool/module.h"
#include "Dispatcher_fiber.hpp"

extern int create_server(lua_State *state);
extern int run_request_fiber(lua_State *state);

class RTSIB_Server;

struct Incoming_request : RTSIB_event, std::enable_shared_from_this<Incoming_request> {
    Incoming_request(RTSIB_Server *a_server, void *a_msg, unsigned a_request_id);
    ~Incoming_request();

    void process() override;
    void process_through_lua() override;

    RTSIB_Server *rtsib_server {nullptr};
    void *msg {nullptr};
    unsigned int request_id {0};
    fiber *_fiber {nullptr};
};

class RTSIB_Callback {
public:
    virtual ~RTSIB_Callback() {}
    virtual void on_request(void *msg, const unsigned r_req_id) = 0;
};

class RTSIB_Server : public RTSIB_Callback {
public:
    RTSIB_Server(std::string a_port_type, std::string cb);
    ~RTSIB_Server();

    void on_request(void *msg, const unsigned r_req_id);

    void set_context_event(std::shared_ptr<Incoming_request> const& ev);
    int on_incoming_request();
    int on_incoming_request(lua_State *state, int request_id);

private:
    std::string port_type;
    std::string lua_callback;
    char log_prefix[128];

    struct Fiber_context {
        std::shared_ptr<Incoming_request> event;
    }   fc;
};

extern RTSIB_Server *g_rtsib_server;

#endif //LUA_RTSIB_RTSIB_SERVER_HPP
