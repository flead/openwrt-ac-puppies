local ski = require("ski")
local log = require("log")
local board = require("board")
local js = require("cjson.safe")
local rpccli = require("rpccli")
local common = require("common")
local cfglib = require("cfglib")

local read, save_safe, arr2map = common.read, common.save_safe, common.arr2map

local udp_map = {}
local udpsrv, mqtt, dbrpc, reply

local function init(u, p)
	udpsrv, mqtt = u, p
	reply = cfglib.gen_reply(udpsrv)
	dbrpc = rpccli.new(mqtt, "a/local/database_srv")
end

udp_map["iface_get"] = function(p, ip, port)
	local r = board.load()
	local ports, options, networks = r.ports, r.options, r.networks
	local path = "/etc/config/network.json"
	local s = read(path) 	assert(s)
	local m = js.decode(s) 	assert(m)
	local net_name, net_cfg = m.name, m.network

	local custom_map = {}
	if net_name ~= "custom" then
		local lan0, wan0 = {}, {#ports}
		for i = 1, #ports - 1 do
			table.insert(lan0, i)
		end
		custom_map.lan0,custom_map.wan0 = lan0, wan0
	else
		for iface, r in pairs(net_cfg) do
			custom_map[iface] = r.ports
		end
	end
	table.insert(options,  {name = "custom", map = custom_map})
	local res = {ports = ports, options = options, networks = networks, network = m}
	reply(ip, port, 0, res)
end

udp_map["iface_list"] = function(p, ip, port)
	local path = "/etc/config/network.json"
	local s = read(path) 	assert(s)
	local m = js.decode(s) 	assert(m)
	local net_name, net_cfg = m.name, m.network

	local ifaces = {}
	for iface, _ in pairs(net_cfg) do
		table.insert(ifaces, iface)
	end
	local res = ifaces
	reply(ip, port, 0, res)
end

local function compare_ports(omap, nmap)
	for iface, oports in pairs(omap) do
		local r = nmap[iface]
		if not (r and r.ports) then
			return nil, "invalid param"
		end

		for i, op in ipairs(oports) do
			if op ~= r.ports[i] then
				return nil, "invalid port"
			end
		end
	end

	for iface, r in pairs(nmap) do
		local nports = r.ports
		if not nports then
			return nil, "invalid param"
		end

		local oports = omap[iface]
		if not oports then
			return nil, "invalid param"
		end

		for i, np in ipairs(nports) do
			if np ~= oports[i] then
				return nil, "invalid port"
			end
		end
	end

	return true
end

udp_map["iface_set"] = function(p, ip, port)
	local config = p.network
	local name, network = config.name, config.network
	local r = board.load()
	local options = arr2map(r.options, "name")

	-- check ports
	if name ~= "custom" then
		local opt = options[name]
		if not opt then
			return reply(ip, port, 1, "invaild name")
		end

		local omap = opt.map
		local r, e = compare_ports(omap, network)
		if not r then
			return reply(ip, port, 1, e)
		end
	end

	local _ = network and save_safe("/etc/config/network.json", js.encode(config))
	reply(ip, port, 0, "ok")
	mqtt:publish("a/local/performer", js.encode({pld = {cmd = "network"}}))
end

return {init = init, dispatch_udp = cfglib.gen_dispatch_udp(udp_map)}
