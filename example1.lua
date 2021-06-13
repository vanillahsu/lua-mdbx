local mdbx = require("mdbx")
local m = mdbx.open("test")

m:set("test", "string")
print(m:get("test"))
for i = 1,10000000 do
    m:get("test")
end
