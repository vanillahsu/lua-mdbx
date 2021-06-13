local mdbx = require("mdbx")
local m = mdbx.open("test")
print(m)

m:set("test", "string")
