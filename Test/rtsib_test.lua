---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by bercut.
--- DateTime: 15.12.2020 17:09
---

data_dir = "data"

box.cfg({
    log_level    = 7
    --, log        = 'rtsib_test_log.txt'
    , wal_dir    = data_dir
    , memtx_dir  = data_dir
    , vinyl_dir  = data_dir
    })

box.schema.sequence.create('oms_order_sqn', {if_not_exists = true})

fiber = require("fiber")
rtsib = require("rtsib")
log = require("log")

--rpc = require("RemoteInstanceConnection").RemoteInstanceConnection("127.0.0.1:3302")

orderNum = 0

function onRequestSync(request)
    print("onRequestSync("..tostring(request)..") called")
    return request + 1000
end

function onRequestFiber(request)
    print("onRequestFiber("..tostring(request)..") called")
    orderNum = orderNum + 1

    local n = orderNum
    local name = "rtsib_test: onRequestFiber #" .. tostring(n)
    print(name .. ": run(" .. request .. ")")

    local f = fiber.new(function()
        print(name .. ": fiber run")
        local seqno = box.sequence.oms_order_sqn:next()
        print(name .. ": fiber end(" .. seqno .. ")")
        return seqno
    end)

    f:set_joinable(true)
    local res, seqno = f:join()

    print(name .. ": end")
    return res and seqno or -1
end

function onRequestFiberCond(request)
    --print("onRequestFiberCond("..tostring(request)..") called")

    local name = "rtsib_test: onRequestFiberCond #" .. tostring(request)
    print(name .. ": run(" .. request .. ")")
    local fc = fiber.cond()

    local f = fiber.create(function()
        print(name .. ": fiber run")
        fiber.sleep(0)
        fc:signal()
        print(name .. ": fiber end")
    end)

    print(name .. ": wait for cond")
    local res = fc:wait(7)
    --if res then
    --    print(name .. ": cond signalled")
    --else
    --    print(name .. ": cond timeout")
    --end

    print(name .. ": return " .. request)
    return request
end

---crashes
function onRequestPcall(request)
    print("onRequestPcall("..tostring(request)..") called")
    orderNum = orderNum + 1

    local n = orderNum
    local productOfferingId = "POserv--5"
    local data = rpc:callFunction("API.ProductCatalog.getProductOffering", productOfferingId)
    
    return n
end

--rtsib.create_server("trnl", "onRequestSync") -- all's good
--rtsib.create_server("trnl", "onRequestFiber") -- all's good
rtsib.create_server("trnl", "onRequestFiberCond")
--rtsib.create_server("trnl", "onRequestPcall") -- all's good

disp_fiber = fiber.create(function ()
    rtsib.run_dispatcher_fiber()
end)

require("console").start()
os.exit()

