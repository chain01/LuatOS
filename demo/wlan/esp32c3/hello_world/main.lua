-- LuaTools需要PROJECT和VERSION这两个信息
PROJECT = "wifidemo"
VERSION = "1.0.0"

-- 引入必要的库文件(lua编写), 内部库不需要require
sys = require("sys")
require("sysplus")

-- sys.subscribe("WLAN_READY", function ()
--     print("!!! wlan ready event !!!")
-- end)

-- sys.taskInit(function()
--     while 1 do
--         sys.wait(5000)
--         log.info("lua", rtos.meminfo())
--         log.info("sys", rtos.meminfo("sys"))
--     end

-- end)

sys.taskInit(function()
    sys.wait(1000)
    wlan.init()
    wlan.connect("uiot", "1234567890")
    log.info("wlan", "wait for IP_READY")
    sys.waitUntil("IP_READY", 30000)
    if wlan.ready() then
        log.info("wlan", "ready !!")
        sys.wait(100)
        -- local url = "http://ip.nutz.cn/json"
        local url = "http://nutzam.com/1.txt"
        local code, headers, body = http2.request("GET", url).wait()
        log.info("http", code, json.encode(headers), #body)
    else
        print("wlan NOT ready!!!!")
    end
end)

-- 用户代码已结束---------------------------------------------
-- 结尾总是这一句
sys.run()
-- sys.run()之后后面不要加任何语句!!!!!
