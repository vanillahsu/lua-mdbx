local mdbx = require("mdbx")
local inspect = require("inspect")
local m = mdbx.env("/tmp/test")
print(inspect.inspect(m))

print(m:get_path())
print(m:get_fd())
print(m:get_maxdbs())
-- local x, err = m:set_option(mdbx.MDBX_opt_max_readers, 10)
-- print(x)
-- print(err)
print(m:get_maxreaders())
-- local txn, err = m:begin_transaction(mdbx.MDBX_TXN_RDONLY)
local txn, err = m:begin_transaction()
print(inspect.inspect(txn))
local dbi, err = txn:open_dbi()
print(inspect.inspect(dbi))
local ret, err = dbi:put(txn, "test", 1)
print(ret)
print(err)
--local ret, err = dbi:delete(txn, "test")
--print(ret)
--print(err)
dbi:close(m)
txn:commit()

local txn, err = m:begin_transaction(mdbx.MDBX_TXN_RDONLY)
local dbi, err = txn:open_dbi()

for i = 1,10000000 do
    num = dbi:get(txn, "test")
end
dbi:close(m)
txn:abort()
-- m:set("test", "string")
-- print(m:get("test"))
-- for i = 1,10000000 do
--     m:get("test")
-- end
