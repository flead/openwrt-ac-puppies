local js = require("cjson.safe")
local rds = require("common.rds")
local r1 = require("common.log").real1

local function reply_e(e)
	ngx.say(js.encode({status = 1, data = e}))
end

local function reply(d)
	ngx.say(js.encode({status = 0, data = d}))
end

local function check_method_token(expect_method, token)
	if expect_method ~= ngx.req.get_method() then
		return nil, "invalid request"
	end

	if not (token and #token == 32) then
		return nil, "invalid token"
	end

	return true
end

local auth_index = 9
local token_ttl_code = [[
	local pc, index, token = redis.call, ARGV[1], ARGV[2]
	local r = pc("SELECT", index) 		assert(r.ok == "OK")
	r = pc("TTL", "admin_" .. token)
	return r
]]

local function validate_token(token)
	local left, e = rds.query(function(rds)
		return rds:eval(token_ttl_code, 0, auth_index, token)
	end)

	if not left then
		return nil, "redis error"
	end

	if left < 300 then
		return nil, "timeout " .. left
	end

	return true
end


local function gen_validate_num(min, max)
	return function(v)
		local v = tonumber(v)
		return (v and v >= min and v <= max) and v or nil
	end
end

local function gen_validate_str(min, max, match_var_style)
	return function(v)
		if not (#v >= min and #v <= max) then
			return nil
		end
		if match_var_style then
			if not v:find("^[%w%-_#.]+$") then
				return nil
			end
		end
		return v
	end
end

local default_validator = {
	page = gen_validate_num(1, 100000),
	count = gen_validate_num(10, 100),
	desc = function(v) return v and 1 or nil end,
}

local function validate_get(fields)
	local p = ngx.req.get_uri_args()
	local token = p.token
	local r, e = check_method_token("GET", token)
	if not r then
		return nil, e
	end

	local r, e = validate_token(token)
	if not r then
		return nil, e
	end

	if not fields then
		return true
	end

	local m = {}
	for field, f in pairs(fields) do
		local v = p[field]
		if not v then
			return nil, "miss " .. field
		end

		local nv, e = (type(f) == "function" and f or default_validator[field])(v)
		if not nv then
			return nil, e or string.format("invalid %s:%s", field, v)
		end

		m[field] = nv
	end

	local page, count = m.page, m.count
	if page or count then
		if not (page and count) then
			return nil, "invalid limit"
		end
	end

	local search, like = m.search, m.like
	if search or like then
		if not (search and like) then
			return nil, "invalid limit"
		end
	end

	return m
end

local function validate_post(fields)
	local p = ngx.req.get_uri_args()
	local token = p.token
	local r, e = check_method_token("POST", token)
	if not r then
		return nil, e
	end

	local r, e = validate_token(token)
	if not r then
		return nil, e
	end

	ngx.req.read_body()
	local p, e = ngx.req.get_post_args()
	if type(p) ~= "table" then
		return nil, e or "invalid post"
	end

	local m = {}

	for field, f in pairs(fields) do
		local v = p[field]
		if not v then
			return nil, "miss " .. field
		end

		local nv, e = f(v)
		if not nv then
			return nil, e or string.format("invalid %s:%s", field, v)
		end

		m[field] = nv
	end

	return m
end

return {
	reply = reply,
	reply_e = reply_e,
	validate_get = validate_get,
	validate_post = validate_post,
	validate_token = validate_token,
	gen_validate_num = gen_validate_num,
	gen_validate_str = gen_validate_str,
	check_method_token = check_method_token,
}


