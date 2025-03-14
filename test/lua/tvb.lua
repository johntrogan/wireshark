----------------------------------------
-- script-name: tvb.lua
-- This tests the Tvb/TvbRange and proto_add_XXX_item API.
----------------------------------------
local testlib = require("testlib")

local FRAME = "frame"
local OTHER = "other"

-- expected number of runs per type
--
-- CHANGE THIS TO MATCH HOW MANY TESTS THERE ARE
--
-- The number of tests in a specific category (other than FRAME) is the
-- number of times execute() is called by any function below testing().
-- From the user's perspective, it can be calculated with the following
-- formula:
--
-- N = number of execute() you call +
--     number of verifyFields() * (1 + number of fields) +
--     number of verifyResults() * (1 + 2 * number of values)
--
-- if one happens to know the number of fields and the number of values.
--
local n_frames = 1
local taptests = { [FRAME]=n_frames, [OTHER]=394*n_frames }

testlib.init(taptests)

------------- test script ------------

----------------------------------------
-- creates a Proto object for our testing
local test_proto = Proto("test","Test Protocol")

local numinits = 0
function test_proto.init()
    numinits = numinits + 1
    if numinits == 2 then
        testlib.getResults()
    end
end


----------------------------------------
-- a table of all of our Protocol's fields
range_string = {
  { 0, 200, "The first part" },
  { 201, 233, "The second part" },
  { 234, 255, "The last part" },
}

local testfield =
{
    basic =
    {
        STRING         = ProtoField.string ("test.basic.string",  "Basic string"),
        BOOLEAN        = ProtoField.bool   ("test.basic.boolean", "Basic boolean", 16, {"yes","no"}, 0x0001),
        UINT8          = ProtoField.uint8  ("test.basic.uint8",   "Basic uint8 with range string", base.RANGE_STRING, range_string ),
        UINT16         = ProtoField.uint16 ("test.basic.uint16",  "Basic uint16"),
        UINT32         = ProtoField.uint32 ("test.basic.uint32",  "Basic uint32 test with a unit string", base.UINT_STRING, { "femtoFarads" }),
        INT24          = ProtoField.int24  ("test.basic.uint24",  "Basic uint24"),
        BYTES          = ProtoField.bytes  ("test.basic.bytes",   "Basic Bytes"),
        UINT_BYTES     = ProtoField.ubytes ("test.basic.ubytes",  "Basic Uint Bytes"),
        OID            = ProtoField.oid    ("test.basic.oid",     "Basic OID"),
        REL_OID        = ProtoField.rel_oid("test.basic.rel_oid", "Basic Relative OID"),
        ABSOLUTE_LOCAL = ProtoField.absolute_time("test.basic.absolute.local","Basic absolute local"),
        ABSOLUTE_UTC   = ProtoField.absolute_time("test.basic.absolute.utc",  "Basic absolute utc", base.UTC),
        IPv4           = ProtoField.ipv4   ("test.basic.ipv4",    "Basic ipv4 address"),
        IPv6           = ProtoField.ipv6   ("test.basic.ipv6",    "Basic ipv6 address"),
        ETHER          = ProtoField.ether  ("test.basic.ether",   "Basic ethernet address"),
        -- GUID           = ProtoField.guid   ("test.basic.guid",    "Basic GUID"),
    },

    time =
    {
        ABSOLUTE_LOCAL = ProtoField.absolute_time("test.time.absolute.local","Time absolute local"),
        ABSOLUTE_UTC   = ProtoField.absolute_time("test.time.absolute.utc",  "Time absolute utc", base.UTC),
    },

    bytes =
    {
        BYTES      = ProtoField.bytes  ("test.bytes.bytes",   "Bytes"),
        UINT_BYTES = ProtoField.ubytes ("test.bytes.ubytes",  "Uint Bytes"),
        OID        = ProtoField.oid    ("test.bytes.oid",     "OID"),
        REL_OID    = ProtoField.rel_oid("test.bytes.rel_oid", "Relative OID"),
        -- GUID       = ProtoField.guid   ("test.bytes.guid",    "GUID"),
    },
}

-- create a flat array table of the above that can be registered
local pfields = {}
for _,t in pairs(testfield) do
    for k,v in pairs(t) do
        pfields[#pfields+1] = v
    end
end

-- register them
test_proto.fields = pfields

print("test_proto ProtoFields registered")


local getfield =
{
    basic =
    {
        STRING         = Field.new ("test.basic.string"),
        BOOLEAN        = Field.new ("test.basic.boolean"),
        UINT8          = Field.new ("test.basic.uint8"),
        UINT16         = Field.new ("test.basic.uint16"),
        INT24          = Field.new ("test.basic.uint24"),
        BYTES          = Field.new ("test.basic.bytes"),
        UINT_BYTES     = Field.new ("test.basic.ubytes"),
        OID            = Field.new ("test.basic.oid"),
        REL_OID        = Field.new ("test.basic.rel_oid"),
        ABSOLUTE_LOCAL = Field.new ("test.basic.absolute.local"),
        ABSOLUTE_UTC   = Field.new ("test.basic.absolute.utc"),
        IPv4           = Field.new ("test.basic.ipv4"),
        IPv6           = Field.new ("test.basic.ipv6"),
        ETHER          = Field.new ("test.basic.ether"),
        -- GUID           = Field.new ("test.basic.guid"),
    },

    time =
    {
        ABSOLUTE_LOCAL = Field.new ("test.time.absolute.local"),
        ABSOLUTE_UTC   = Field.new ("test.time.absolute.utc"),
    },

    bytes =
    {
        BYTES      = Field.new ("test.bytes.bytes"),
        UINT_BYTES = Field.new ("test.bytes.ubytes"),
        OID        = Field.new ("test.bytes.oid"),
        REL_OID    = Field.new ("test.bytes.rel_oid"),
        -- GUID       = Field.new ("test.bytes.guid"),
    },
}

print("test_proto Fields created")

local function addMatchFields(match_fields, ... )
    match_fields[#match_fields + 1] = { ... }
end

local function getFieldInfos(name)
    local base, field = name:match("([^.]+)%.(.+)")
    if not base or not field then
        error("failed to get base.field from '" .. name .. "'")
    end
    local t = { getfield[base][field]() }
    return t
end

local function verifyFields(name, match_fields)
    local finfos = getFieldInfos(name)

    testlib.test(OTHER, "verify-fields-size-" .. name, #finfos == #match_fields,
             "#finfos=" .. #finfos .. ", #match_fields=" .. #match_fields)

    for i, t in ipairs(match_fields) do
        if type(t) ~= 'table' then
            error("verifyFields didn't get a table inside the matches table")
        end
        if #t ~= 1 then
            error("verifyFields matches table's table is not size 1")
        end
        local result = finfos[i]()
        local value  = t[1]
        print(
                name .. " got:",
                "\n\tfinfos [" .. i .. "]='" .. tostring( result ) .. "'",
                "\n\tmatches[" .. i .. "]='" .. tostring( value  ) .. "'"
             )
        testlib.test(OTHER, "verify-fields-value-" .. name .. "-" .. i, result == value )
    end
end


local function addMatchValues(match_values, ... )
    match_values[#match_values + 1] = { ... }
end

local function addMatchFieldValues(match_fields, match_values, match_both, ...)
    addMatchFields(match_fields, match_both)
    addMatchValues(match_values, match_both, ...)
end

local result_values = {}
local function resetResults()
    result_values = {}
end

local function treeAddPField(...)
    local t = { pcall ( TreeItem.add_packet_field, ... ) }
    if t[1] == nil then
        return nil, t[2]
    end
    -- it gives back a TreeItem, then the results
    if typeof(t[2]) ~= 'TreeItem' then
        return nil, "did not get a TreeItem returned from TreeItem.add_packet_field, "..
                    "got a '" .. typeof(t[2]) .."'"
    end

    if #t ~= 4 then
        return nil, "did not get 3 return values from TreeItem.add_packet_field"
    end

    result_values[#result_values + 1] = { t[3], t[4] }

    return true
end

local function verifyResults(name, match_values)
    testlib.test(OTHER, "verify-results-size-" .. name, #result_values == #match_values,
             "#result_values=" .. #result_values ..
             ", #match_values=" .. #match_values)

    for j, t in ipairs(match_values) do
        if type(t) ~= 'table' then
            error("verifyResults didn't get a table inside the matches table")
        end
        for i, match in ipairs(t) do
            local r = result_values[j][i]
            print(
                    name .. " got:",
                    "\n\tresults[" .. j .. "][" .. i .. "]='" .. tostring( r ) .. "'",
                    "\n\tmatches[" .. j .. "][" .. i .. "]='" .. tostring( match ) .. "'"
                 )
            local result_type, match_type
            if type(match) == 'userdata' then
                match_type = typeof(match)
            else
                match_type = type(match)
            end
            if type(r) == 'userdata' then
                result_type = typeof(r)
            else
                result_type = type(r)
            end
            testlib.test(OTHER, "verify-results-type-" .. name .. "-" .. i, result_type == match_type )
            testlib.test(OTHER, "verify-results-value-" .. name .. "-" .. i, r == match )
        end
    end
end

-- Compute the difference in seconds between local time and UTC
-- from http://lua-users.org/wiki/TimeZone
local function get_timezone()
  local now = os.time()
  return os.difftime(now, os.time(os.date("!*t", now)))
end
local timezone = get_timezone()
print ("timezone = " .. timezone)

----------------------------------------
-- The following creates the callback function for the dissector.
-- The 'tvbuf' is a Tvb object, 'pktinfo' is a Pinfo object, and 'root' is a TreeItem object.
function test_proto.dissector(tvbuf,pktinfo,root)

    testlib.countPacket(FRAME)
    testlib.countPacket(OTHER)

    testlib.testing(OTHER, "Basic string")

    local tree = root:add(test_proto, tvbuf:range(0,tvbuf:len()))

    -- create a fake Tvb to use for testing
    local teststring = "this is the string for the first test"
    local bytearray  = ByteArray.new(teststring, true)
    local tvb_string = bytearray:tvb("Basic string")

    local function callTreeAdd(tree,...)
        tree:add(...)
    end

    local string_match_fields = {}

    testlib.test(OTHER, "basic-tvb_get_string", tvb_string:range():string() == teststring )

    testlib.test(OTHER, "basic-string", tree:add(testfield.basic.STRING, tvb_string:range(0,tvb_string:len())) ~= nil )
    addMatchFields(string_match_fields, teststring)

    testlib.test(OTHER, "basic-string", pcall (callTreeAdd, tree, testfield.basic.STRING, tvb_string:range() ) )
    addMatchFields(string_match_fields, teststring)

    verifyFields("basic.STRING", string_match_fields)

----------------------------------------
    testlib.testing(OTHER, "Basic boolean")

    local barray_bytes_hex  = "00FF00018000"
    local barray_bytes      = ByteArray.new(barray_bytes_hex)
    local tvb_bytes         = barray_bytes:tvb("Basic bytes")
    local bool_match_fields = {}

    testlib.test(OTHER, "basic-boolean", pcall (callTreeAdd, tree, testfield.basic.BOOLEAN, tvb_bytes:range(0,2)) )
    addMatchFields(bool_match_fields, true)

    testlib.test(OTHER, "basic-boolean", pcall (callTreeAdd, tree, testfield.basic.BOOLEAN, tvb_bytes:range(2,2)) )
    addMatchFields(bool_match_fields, true)

    testlib.test(OTHER, "basic-boolean", pcall (callTreeAdd, tree, testfield.basic.BOOLEAN, tvb_bytes:range(4,2)) )
    addMatchFields(bool_match_fields, false)

    verifyFields("basic.BOOLEAN", bool_match_fields )

----------------------------------------
    testlib.testing(OTHER, "Basic uint16")

    local uint16_match_fields = {}

    testlib.test(OTHER, "basic-uint16", pcall (callTreeAdd, tree, testfield.basic.UINT16, tvb_bytes:range(0,2)) )
    addMatchFields(uint16_match_fields, 255)

    testlib.test(OTHER, "basic-uint16", pcall (callTreeAdd, tree, testfield.basic.UINT16, tvb_bytes:range(2,2)) )
    addMatchFields(uint16_match_fields, 1)

    testlib.test(OTHER, "basic-uint16", pcall (callTreeAdd, tree, testfield.basic.UINT16, tvb_bytes:range(4,2)) )
    addMatchFields(uint16_match_fields, 32768)

    verifyFields("basic.UINT16", uint16_match_fields)

----------------------------------------
    testlib.testing(OTHER, "Basic uint16-le")

    local function callTreeAddLE(tree,...)
        tree:add_le(...)
    end

    testlib.test(OTHER, "basic-uint16-le", pcall (callTreeAddLE, tree, testfield.basic.UINT16, tvb_bytes:range(0,2)) )
    addMatchFields(uint16_match_fields, 65280)

    testlib.test(OTHER, "basic-uint16-le", pcall (callTreeAddLE, tree, testfield.basic.UINT16, tvb_bytes:range(2,2)) )
    addMatchFields(uint16_match_fields, 256)

    testlib.test(OTHER, "basic-uint16-le", pcall (callTreeAddLE, tree, testfield.basic.UINT16, tvb_bytes:range(4,2)) )
    addMatchFields(uint16_match_fields, 128)

    verifyFields("basic.UINT16", uint16_match_fields)

----------------------------------------
    testlib.testing(OTHER, "Basic int24")

    local int24_match_fields = {}

    testlib.test(OTHER, "basic-int24", pcall (callTreeAdd, tree, testfield.basic.INT24, tvb_bytes:range(0,3)) )
    addMatchFields(int24_match_fields, 65280)

    testlib.test(OTHER, "basic-int24", pcall (callTreeAdd, tree, testfield.basic.INT24, tvb_bytes:range(3,3)) )
    addMatchFields(int24_match_fields, 98304)

    verifyFields("basic.INT24", int24_match_fields)

----------------------------------------
    testlib.testing(OTHER, "Basic int24-le")

    testlib.test(OTHER, "basic-int24", pcall (callTreeAddLE, tree, testfield.basic.INT24, tvb_bytes:range(0,3)) )
    addMatchFields(int24_match_fields, 65280)

    testlib.test(OTHER, "basic-int24", pcall (callTreeAddLE, tree, testfield.basic.INT24, tvb_bytes:range(3,3)) )
    addMatchFields(int24_match_fields, 32769)

    verifyFields("basic.INT24", int24_match_fields)

----------------------------------------
    testlib.testing(OTHER, "Basic bytes")

    local bytes_match_fields = {}

    testlib.test(OTHER, "basic-tvb_get_string_bytes",
             string.lower(tostring(tvb_bytes:range():bytes())) == string.lower(barray_bytes_hex))

    testlib.test(OTHER, "basic-bytes", pcall (callTreeAdd, tree, testfield.basic.BYTES, tvb_bytes:range()) )
    addMatchFields(bytes_match_fields, barray_bytes)

    -- TODO: it's silly that tree:add_packet_field() requires an encoding argument
    --  need to fix that separately in a bug fix
    testlib.test(OTHER, "add_pfield-bytes", treeAddPField(tree, testfield.basic.BYTES,
                                               tvb_bytes:range(), ENC_BIG_ENDIAN))
    addMatchFields(bytes_match_fields, barray_bytes)

    verifyFields("basic.BYTES", bytes_match_fields)

----------------------------------------
    testlib.testing(OTHER, "Basic uint bytes")

    local len_string        = string.format("%02x", barray_bytes:len())
    local barray_uint_bytes = ByteArray.new(len_string) .. barray_bytes
    local tvb_uint_bytes    = barray_uint_bytes:tvb("Basic UINT_BYTES")
    local uint_bytes_match_fields = {}

    testlib.test(OTHER, "basic-uint-bytes", pcall (callTreeAdd, tree, testfield.basic.UINT_BYTES,
                                        tvb_uint_bytes:range(0,1)) )
    addMatchFields(uint_bytes_match_fields, barray_bytes)

    testlib.test(OTHER, "add_pfield-uint-bytes", treeAddPField(tree, testfield.basic.UINT_BYTES,
                                                    tvb_uint_bytes:range(0,1), ENC_BIG_ENDIAN) )
    addMatchFields(uint_bytes_match_fields, barray_bytes)

    verifyFields("basic.UINT_BYTES", uint_bytes_match_fields)

----------------------------------------
    testlib.testing(OTHER, "Basic OID")

    -- note: the tvb being dissected and compared isn't actually a valid OID.
    -- tree:add() and tree:add_packet-field() don't care about its validity right now.

    local oid_match_fields = {}

    testlib.test(OTHER, "basic-oid", pcall(callTreeAdd, tree, testfield.basic.OID, tvb_bytes:range()) )
    addMatchFields(oid_match_fields, barray_bytes)

    testlib.test(OTHER, "add_pfield-oid", treeAddPField(tree, testfield.basic.OID,
                                             tvb_bytes:range(), ENC_BIG_ENDIAN) )
    addMatchFields(oid_match_fields, barray_bytes)

    verifyFields("basic.OID", oid_match_fields)

----------------------------------------
    testlib.testing(OTHER, "Basic REL_OID")

    -- note: the tvb being dissected and compared isn't actually a valid OID.
    -- tree:add() and tree:add_packet-field() don't care about its validity right now.

    local rel_oid_match_fields = {}

    testlib.test(OTHER, "basic-rel-oid", pcall(callTreeAdd, tree, testfield.basic.REL_OID, tvb_bytes:range()))
    addMatchFields(rel_oid_match_fields, barray_bytes)

    testlib.test(OTHER, "add_pfield-rel_oid", treeAddPField(tree, testfield.basic.REL_OID,
                                                 tvb_bytes:range(), ENC_BIG_ENDIAN) )
    addMatchFields(rel_oid_match_fields, barray_bytes)

    verifyFields("basic.REL_OID", rel_oid_match_fields)

    -- TODO: a FT_GUID is not really a ByteArray, so we can't simply treat it as one
    -- local barray_guid       = ByteArray.new("00FF0001 80001234 567890AB CDEF00FF")
    -- local tvb_guid          = barray_guid:tvb("Basic GUID")
    -- local guid_match_fields = {}

    -- testlib.test(OTHER, "basic-guid", pcall(callTreeAdd, tree, testfield.basic.GUID, tvb_guid:range()) )
    -- addMatchFields(guid_match_fields, barray_guid)

    -- testlib.test(OTHER, "add_pfield-guid", treeAddPField(tree, testfield.basic.GUID,
    --                                          tvb_guid:range(), ENC_BIG_ENDIAN) )
    -- addMatchFields(guid_match_fields, barray_guid)

    -- verifyFields("basic.GUID", guid_match_fields)

----------------------------------------
    testlib.testing(OTHER, "tree:add ipv6")

    local tvb = ByteArray.new("20010db8 00000000 0000ff00 00428329"):tvb("IPv6")
    local IPv6 = testfield.basic.IPv6
    local ipv6_match_fields = {}

    testlib.test(OTHER, "ipv6", pcall (callTreeAdd, tree, IPv6, tvb:range(0,16)))
    addMatchFields(ipv6_match_fields, Address.ipv6('2001:0db8:0000:0000:0000:ff00:0042:8329'))

    verifyFields("basic.IPv6", ipv6_match_fields)

----------------------------------------
    testlib.testing(OTHER, "tree:add ipv4")

    local tvb = ByteArray.new("7f000001"):tvb("IPv4")
    local IPv4 = testfield.basic.IPv4
    local ipv4_match_fields = {}

    testlib.test(OTHER, "ipv4", pcall (callTreeAdd, tree, IPv4, tvb:range(0,4)))
    addMatchFields(ipv4_match_fields, Address.ip('127.0.0.1'))

    -- TODO: currently, tree:add_le only works for numeric values, not IPv4
    -- addresses. Test this in the future.

    -- testlib.test(OTHER, "ipv4", pcall (callTreeAddLE, tree, IPv4, tvb:range(0,4)))
    -- addMatchFields(ipv4_match_fields, Address.ip('1.0.0.127'))

    verifyFields("basic.IPv4", ipv4_match_fields)

----------------------------------------
    testlib.testing(OTHER, "tree:add ether")

    local tvb = ByteArray.new("010203040506"):tvb("Ether")
    local tvb0 = ByteArray.new("000000000000"):tvb("Ether0")
    local ether = testfield.basic.ETHER
    local ether_match_fields = {}

    testlib.test(OTHER, "ether", pcall (callTreeAdd, tree, ether, tvb:range(0,6)))
    addMatchFields(ether_match_fields, Address.ether('01:02:03:04:05:06'))

    testlib.test(OTHER, "ether0", pcall (callTreeAdd, tree, ether, tvb0:range(0,6)))
    addMatchFields(ether_match_fields, Address.ether('11:22:33'))

    verifyFields("basic.ETHER", ether_match_fields)

----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field Bytes")

    resetResults()
    bytes_match_fields = {}
    local bytes_match_values = {}

    -- something to make this easier to read
    local function addMatch(...)
        addMatchFieldValues(bytes_match_fields, bytes_match_values, ...)
    end

    local bytesstring1 =   "deadbeef0123456789DEADBEEFabcdef"
    local bytesstring = ByteArray.new(bytesstring1) -- the binary version of above, for comparing
    local bytestvb1 = ByteArray.new(bytesstring1, true):tvb("Bytes hex-string 1")
    local bytesstring2 = "  de:ad:be:ef:01:23:45:67:89:DE:AD:BE:EF:ab:cd:ef"
    local bytestvb2 = ByteArray.new(bytesstring2 .. "-f0-00 foobar", true):tvb("Bytes hex-string 2")

    local bytestvb1_decode = bytestvb1:range():bytes(ENC_STR_HEX + ENC_SEP_NONE + ENC_SEP_COLON + ENC_SEP_DASH)
    testlib.test(OTHER, "tvb_get_string_bytes", string.lower(tostring(bytestvb1_decode)) == string.lower(tostring(bytesstring1)))

    testlib.test(OTHER, "add_pfield-bytes1", treeAddPField(tree, testfield.bytes.BYTES,
                                               bytestvb1:range(),
                                               ENC_STR_HEX + ENC_SEP_NONE +
                                               ENC_SEP_COLON + ENC_SEP_DASH))
    addMatch(bytesstring, string.len(bytesstring1))

    testlib.test(OTHER, "add_pfield-bytes2", treeAddPField(tree, testfield.bytes.BYTES,
                                               bytestvb2:range(),
                                               ENC_STR_HEX + ENC_SEP_NONE +
                                               ENC_SEP_COLON + ENC_SEP_DASH))
    addMatch(bytesstring, string.len(bytesstring2))

    verifyResults("add_pfield-bytes", bytes_match_values)
    verifyFields("bytes.BYTES", bytes_match_fields)

    -- extra test of ByteArray
    local b64padded = ByteArray.new("dGVzdA==", true):base64_decode():raw()
    local b64unpadded = ByteArray.new("dGVzdA", true):base64_decode():raw()
    testlib.test(OTHER, "bytearray_base64_padded", b64padded == "test")
    testlib.test(OTHER, "bytearray_base64_unpadded", b64unpadded == "test")


----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field OID")

    resetResults()
    bytes_match_fields = {}
    bytes_match_values = {}

    testlib.test(OTHER, "add_pfield-oid1", treeAddPField(tree, testfield.bytes.OID,
                                               bytestvb1:range(),
                                               ENC_STR_HEX + ENC_SEP_NONE +
                                               ENC_SEP_COLON + ENC_SEP_DASH))
    addMatch(bytesstring, string.len(bytesstring1))

    testlib.test(OTHER, "add_pfield-oid2", treeAddPField(tree, testfield.bytes.OID,
                                               bytestvb2:range(),
                                               ENC_STR_HEX + ENC_SEP_NONE +
                                               ENC_SEP_COLON + ENC_SEP_DASH))
    addMatch(bytesstring, string.len(bytesstring2))

    verifyResults("add_pfield-oid", bytes_match_values)
    verifyFields("bytes.OID", bytes_match_fields)


----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field REL_OID")

    resetResults()
    bytes_match_fields = {}
    bytes_match_values = {}

    testlib.test(OTHER, "add_pfield-rel_oid1", treeAddPField(tree, testfield.bytes.REL_OID,
                                               bytestvb1:range(),
                                               ENC_STR_HEX + ENC_SEP_NONE +
                                               ENC_SEP_COLON + ENC_SEP_DASH))
    addMatch(bytesstring, string.len(bytesstring1))

    testlib.test(OTHER, "add_pfield-rel_oid2", treeAddPField(tree, testfield.bytes.REL_OID,
                                               bytestvb2:range(),
                                               ENC_STR_HEX + ENC_SEP_NONE +
                                               ENC_SEP_COLON + ENC_SEP_DASH))
    addMatch(bytesstring, string.len(bytesstring2))

    verifyResults("add_pfield-rel_oid", bytes_match_values)
    verifyFields("bytes.REL_OID", bytes_match_fields)


----------------------------------------
    testlib.testing(OTHER, "tree:add Time")

    local tvb = ByteArray.new("00000000 00000000 0000FF0F 00FF000F"):tvb("Time")
    local ALOCAL = testfield.time.ABSOLUTE_LOCAL
    local alocal_match_fields = {}

    testlib.test(OTHER, "time-local",    pcall (callTreeAdd,   tree, ALOCAL, tvb:range(0,8)) )
    addMatchFields(alocal_match_fields, NSTime())

    testlib.test(OTHER, "time-local",    pcall (callTreeAdd,   tree, ALOCAL, tvb:range(8,8)) )
    addMatchFields(alocal_match_fields, NSTime( 0x0000FF0F, 0x00FF000F) )

    testlib.test(OTHER, "time-local-le", pcall (callTreeAddLE, tree, ALOCAL, tvb:range(0,8)) )
    addMatchFields(alocal_match_fields, NSTime())

    testlib.test(OTHER, "time-local-le", pcall (callTreeAddLE, tree, ALOCAL, tvb:range(8,8)) )
    addMatchFields(alocal_match_fields, NSTime( 0x0FFF0000, 0x0F00FF00 ) )

    verifyFields("time.ABSOLUTE_LOCAL", alocal_match_fields)

    local AUTC = testfield.time.ABSOLUTE_UTC
    local autc_match_fields = {}

    testlib.test(OTHER, "time-utc",    pcall (callTreeAdd,   tree, AUTC, tvb:range(0,8)) )
    addMatchFields(autc_match_fields, NSTime())

    testlib.test(OTHER, "time-utc",    pcall (callTreeAdd,   tree, AUTC, tvb:range(8,8)) )
    addMatchFields(autc_match_fields, NSTime( 0x0000FF0F, 0x00FF000F) )

    testlib.test(OTHER, "time-utc-le", pcall (callTreeAddLE, tree, AUTC, tvb:range(0,8)) )
    addMatchFields(autc_match_fields, NSTime())

    testlib.test(OTHER, "time-utc-le", pcall (callTreeAddLE, tree, AUTC, tvb:range(8,8)) )
    addMatchFields(autc_match_fields, NSTime( 0x0FFF0000, 0x0F00FF00 ) )

    verifyFields("time.ABSOLUTE_UTC", autc_match_fields )

----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field Time bytes")

    resetResults()
    local autc_match_values = {}

    -- something to make this easier to read
    addMatch = function(...)
        addMatchFieldValues(autc_match_fields, autc_match_values, ...)
    end

    -- tree:add_packet_field(ALOCAL, tvb:range(0,8), ENC_BIG_ENDIAN)
    testlib.test(OTHER, "add_pfield-time-bytes-local",    treeAddPField ( tree, AUTC, tvb:range(0,8), ENC_BIG_ENDIAN) )
    addMatch( NSTime(), 8)

    testlib.test(OTHER, "add_pfield-time-bytes-local",    treeAddPField ( tree, AUTC, tvb:range(8,8), ENC_BIG_ENDIAN) )
    addMatch( NSTime( 0x0000FF0F, 0x00FF000F), 16)

    testlib.test(OTHER, "add_pfield-time-bytes-local-le", treeAddPField ( tree, AUTC, tvb:range(0,8), ENC_LITTLE_ENDIAN) )
    addMatch( NSTime(), 8)

    testlib.test(OTHER, "add_pfield-time-bytes-local-le", treeAddPField ( tree, AUTC, tvb:range(8,8), ENC_LITTLE_ENDIAN) )
    addMatch( NSTime( 0x0FFF0000, 0x0F00FF00 ), 16)

    verifyFields("time.ABSOLUTE_UTC", autc_match_fields)

    verifyResults("add_pfield-time-bytes-local", autc_match_values)

----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field Time string ENC_ISO_8601_DATE_TIME")

    resetResults()
    autc_match_values = {}

    local datetimestring1 =   "2013-03-01T22:14:48+00:00" -- this is 1362176088 seconds epoch time
    local tvb1 = ByteArray.new(datetimestring1, true):tvb("Date_Time string 1")
    local datetimestring2 = "  2013-03-02T03:14:48+05:00" -- this is 1362176088 seconds epoch time
    local tvb2 = ByteArray.new(datetimestring2 .. "  foobar", true):tvb("Date_Time string 2")
    local datetimestring3 = "  2013-03-01T16:44-05:30"    -- this is 1362176040 seconds epoch time
    local tvb3 = ByteArray.new(datetimestring3, true):tvb("Date_Time string 3")
    local datetimestring4 =   "2013-03-02T01:44:00+03:30" -- this is 1362176040 seconds epoch time
    local tvb4 = ByteArray.new(datetimestring4, true):tvb("Date_Time string 4")
    local datetimestring5 =   "2013-03-01T22:14:48Z"      -- this is 1362176088 seconds epoch time
    local tvb5 = ByteArray.new(datetimestring5, true):tvb("Date_Time string 5")
    local datetimestring6 =   "2013-03-01T22:14Z"         -- this is 1362176040 seconds epoch time
    local tvb6 = ByteArray.new(datetimestring6, true):tvb("Date_Time string 6")

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb1:range(), ENC_ISO_8601_DATE_TIME) )
    addMatch( NSTime( 1362176088, 0), string.len(datetimestring1))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb2:range(), ENC_ISO_8601_DATE_TIME) )
    addMatch( NSTime( 1362176088, 0), string.len(datetimestring2))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb3:range(), ENC_ISO_8601_DATE_TIME) )
    addMatch( NSTime( 1362176040, 0), string.len(datetimestring3))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb4:range(), ENC_ISO_8601_DATE_TIME) )
    addMatch( NSTime( 1362176040, 0), string.len(datetimestring4))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb5:range(), ENC_ISO_8601_DATE_TIME) )
    addMatch( NSTime( 1362176088, 0), string.len(datetimestring5))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb6:range(), ENC_ISO_8601_DATE_TIME) )
    addMatch( NSTime( 1362176040, 0), string.len(datetimestring6))

    verifyFields("time.ABSOLUTE_UTC", autc_match_fields)

    verifyResults("add_pfield-datetime-local", autc_match_values)

----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field Time string ENC_ISO_8601_DATE")

    resetResults()
    autc_match_values = {}

    local datestring1 =   "2013-03-01"  -- this is 1362096000 seconds epoch time
    local d_tvb1 = ByteArray.new(datestring1, true):tvb("Date string 1")
    local datestring2 = "  2013-03-01"  -- this is 1362096000 seconds epoch time
    local d_tvb2 = ByteArray.new(datestring2 .. "  foobar", true):tvb("Date string 2")

    testlib.test(OTHER, "add_pfield-date-local", treeAddPField ( tree, AUTC, d_tvb1:range(), ENC_ISO_8601_DATE) )
    addMatch( NSTime( 1362096000, 0), string.len(datestring1))

    testlib.test(OTHER, "add_pfield-date-local", treeAddPField ( tree, AUTC, d_tvb2:range(), ENC_ISO_8601_DATE) )
    addMatch( NSTime( 1362096000, 0), string.len(datestring2))

    verifyFields("time.ABSOLUTE_UTC", autc_match_fields)

    verifyResults("add_pfield-date-local", autc_match_values)

----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field Time string ENC_ISO_8601_TIME")

    resetResults()
    autc_match_values = {}

    local timestring1 =   "22:14:48"  -- this is 80088 seconds
    local t_tvb1 = ByteArray.new(timestring1, true):tvb("Time string 1")
    local timestring2 = "  22:14:48"  -- this is 80088 seconds
    local t_tvb2 = ByteArray.new(timestring2 .. "  foobar", true):tvb("Time string 2")

    local now = os.date("!*t")
    now.hour = 22
    now.min  = 14
    now.sec  = 48
    local timebase = os.time( now )
    timebase = timebase + timezone
    print ("timebase = " .. tostring(timebase) .. ", timezone=" .. timezone)

    testlib.test(OTHER, "add_pfield-time-local", treeAddPField ( tree, AUTC, t_tvb1:range(), ENC_ISO_8601_TIME) )
    addMatch( NSTime( timebase, 0), string.len(timestring1))

    testlib.test(OTHER, "add_pfield-time-local", treeAddPField ( tree, AUTC, t_tvb2:range(), ENC_ISO_8601_TIME) )
    addMatch( NSTime( timebase, 0), string.len(timestring2))

    verifyFields("time.ABSOLUTE_UTC", autc_match_fields)

    verifyResults("add_pfield-time-local", autc_match_values)

----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field Time string ENC_IMF_DATE_TIME")

    resetResults()
    autc_match_values = {}

    local imfstring1 =   "Fri, 01 Mar 13 22:14:48 GMT"  -- this is 1362176088 seconds epoch time
    local imf_tvb1 = ByteArray.new(imfstring1, true):tvb("Internet Message Format Time string 1")
    local imfstring2 = "  Fri, 01 Mar 13 22:14:48 GMT"  -- this is 1362176088 seconds epoch time
    local imf_tvb2 = ByteArray.new(imfstring2 .. "  foobar", true):tvb("Internet Message Format Time string 2")
    local imfstring3 =   "Fri, 01 Mar 2013 22:14:48 GMT"  -- this is 1362176088 seconds epoch time
    local imf_tvb3 = ByteArray.new(imfstring3, true):tvb("Internet Message Format Time string 3")
    local imfstring4 = "  Fri, 01 Mar 2013 22:14:48 GMT"  -- this is 1362176088 seconds epoch time
    local imf_tvb4 = ByteArray.new(imfstring4 .. "  foobar", true):tvb("Internet Message Format Time string 4")

    testlib.test(OTHER, "add_pfield-time-local", treeAddPField ( tree, AUTC, imf_tvb1:range(), ENC_IMF_DATE_TIME) )
    addMatch( NSTime( 1362176088, 0), string.len(imfstring1))

    testlib.test(OTHER, "add_pfield-time-local", treeAddPField ( tree, AUTC, imf_tvb2:range(), ENC_IMF_DATE_TIME) )
    addMatch( NSTime( 1362176088, 0), string.len(imfstring2))

    testlib.test(OTHER, "add_pfield-time-local", treeAddPField ( tree, AUTC, imf_tvb3:range(), ENC_IMF_DATE_TIME) )
    addMatch( NSTime( 1362176088, 0), string.len(imfstring3))

    testlib.test(OTHER, "add_pfield-time-local", treeAddPField ( tree, AUTC, imf_tvb4:range(), ENC_IMF_DATE_TIME) )
    addMatch( NSTime( 1362176088, 0), string.len(imfstring4))

    verifyFields("time.ABSOLUTE_UTC", autc_match_fields)

    verifyResults("add_pfield-imf-date-time-local", autc_match_values)

----------------------------------------
    testlib.testing(OTHER, "tree:add_packet_field Time string ENC_ISO_8601_DATE_TIME_BASIC")

    resetResults()
    autc_match_values = {}

    local datetimestring1 =   "20130301T221448+0000" -- this is 1362176088 seconds epoch time
    local tvb1 = ByteArray.new(datetimestring1, true):tvb("Date_Time string 1")
    local datetimestring2 = "  20130301171448-0500" -- this is 1362176088 seconds epoch time
    local tvb2 = ByteArray.new(datetimestring2 .. "  foobar", true):tvb("Date_Time string 2")
    local datetimestring3 = "  20130301T1644-0530"    -- this is 1362176040 seconds epoch time
    local tvb3 = ByteArray.new(datetimestring3, true):tvb("Date_Time string 3")
    local datetimestring4 =   "20130302 014400+0330" -- this is 1362176040 seconds epoch time
    local tvb4 = ByteArray.new(datetimestring4, true):tvb("Date_Time string 4")
    local datetimestring5 =   "20130301T221448Z"      -- this is 1362176088 seconds epoch time
    local tvb5 = ByteArray.new(datetimestring5, true):tvb("Date_Time string 5")
    local datetimestring6 =   "201303012214Z"         -- this is 1362176040 seconds epoch time
    local tvb6 = ByteArray.new(datetimestring6, true):tvb("Date_Time string 6")

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb1:range(), ENC_ISO_8601_DATE_TIME_BASIC) )
    addMatch( NSTime( 1362176088, 0), string.len(datetimestring1))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb2:range(), ENC_ISO_8601_DATE_TIME_BASIC) )
    addMatch( NSTime( 1362176088, 0), string.len(datetimestring2))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb3:range(), ENC_ISO_8601_DATE_TIME_BASIC) )
    addMatch( NSTime( 1362176040, 0), string.len(datetimestring3))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb4:range(), ENC_ISO_8601_DATE_TIME_BASIC) )
    addMatch( NSTime( 1362176040, 0), string.len(datetimestring4))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb5:range(), ENC_ISO_8601_DATE_TIME_BASIC) )
    addMatch( NSTime( 1362176088, 0), string.len(datetimestring5))

    testlib.test(OTHER, "add_pfield-datetime-local", treeAddPField ( tree, AUTC, tvb6:range(), ENC_ISO_8601_DATE_TIME_BASIC) )
    addMatch( NSTime( 1362176040, 0), string.len(datetimestring6))

    verifyFields("time.ABSOLUTE_UTC", autc_match_fields)

    verifyResults("add_pfield-datetime-local", autc_match_values)

----------------------------------------
    testlib.testing(OTHER, "TvbRange subsets")

    resetResults()

    local offset = 5
    local len = 10
    local b_offset = 3
    local b_len = 2
    local range
    local range_raw
    local expected

    -- This is the same data from the "tree:add_packet_field Bytes" test
    -- copied here for clarity
    local bytesstring1 = "deadbeef0123456789DEADBEEFabcdef"
    local bytestvb1 = ByteArray.new(bytesstring1, true):tvb("Bytes hex-string 1")

    -- tvbrange with no offset or length (control test case)
    range = bytestvb1()
    range_raw = range:raw()
    expected = range:bytes():raw()
    testlib.test(OTHER, "tvbrange_raw", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(b_offset)
    expected = range:bytes():raw(b_offset)
    testlib.test(OTHER, "tvbrange_raw_offset", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(0, b_len)
    expected = range:bytes():raw(0, b_len)
    testlib.test(OTHER, "tvbrange_raw_len", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(b_offset, b_len)
    expected = range:bytes():raw(b_offset, b_len)
    testlib.test(OTHER, "tvbrange_raw_offset_len", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))

    -- tvbrange with len only
    range = bytestvb1(0, len)
    range_raw = range:raw()
    expected = range:bytes():raw()
    testlib.test(OTHER, "tvbrange_len_raw", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(b_offset)
    expected = range:bytes():raw(b_offset)
    testlib.test(OTHER, "tvbrange_len_raw_offset", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(0, b_len)
    expected = range:bytes():raw(0, b_len)
    testlib.test(OTHER, "tvbrange_len_raw_len", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(b_offset, b_len)
    expected = range:bytes():raw(b_offset, b_len)
    testlib.test(OTHER, "tvbrange_len_raw_offset_len", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))

    -- tvbrange with offset only
    range = bytestvb1(offset)
    range_raw = range:raw()
    expected = range:bytes():raw()
    testlib.test(OTHER, "tvbrange_offset_raw", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(b_offset)
    expected = range:bytes():raw(b_offset)
    testlib.test(OTHER, "tvbrange_offset_raw_offset", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(0, b_len)
    expected = range:bytes():raw(0, b_len)
    testlib.test(OTHER, "tvbrange_offset_raw_len", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(b_offset, b_len)
    expected = range:bytes():raw(b_offset, b_len)
    testlib.test(OTHER, "tvbrange_offset_raw_offset_len", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))

    -- tvbrange with offset and len
    range = bytestvb1(offset, len)
    range_raw = range:raw()
    expected = range:bytes():raw()
    testlib.test(OTHER, "tvbrange_offset_len_raw", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(b_offset)
    expected = range:bytes():raw(b_offset)
    testlib.test(OTHER, "tvbrange_offset_len_raw_offset", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(0, b_len)
    expected = range:bytes():raw(0, b_len)
    testlib.test(OTHER, "tvbrange_offset_len_raw_len", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    range_raw = range:raw(b_offset, b_len)
    expected = range:bytes():raw(b_offset, b_len)
    testlib.test(OTHER, "tvbrange_offset_len_raw_offset_len", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))

    -- Edge cases with zero length, and different ways to create them
    range = bytestvb1()
    -- Subrange of a range starting with the last offset (zero length)
    local subrange = range:range(range:len())
    range_raw = subrange:raw()
    expected = subrange:bytes():raw()
    testlib.test(OTHER, "tvbrange_subrange_empty_raw", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    -- Create an empty byte array (this is different because the backing
    -- tvb is empty)
    local emptybytestvb1 = ByteArray.new("", true):tvb("Empty bytes hex-string1")
    range = emptybytestvb1()
    range_raw = range:raw()
    expected = range:bytes():raw()
    testlib.test(OTHER, "tvbrange_empty_raw", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))
    -- Convert the range back to an empty tvb
    local emptybytestvb2 = range:tvb()
    expected = emptybytestvb2:bytes():raw()
    testlib.test(OTHER, "tvbrange_empty_tvb_raw", range_raw == expected,
        string.format('range_raw="%s" expected="%s"', range_raw, expected))

----------------------------------------

    testlib.pass(FRAME)
end

----------------------------------------
-- we want to have our protocol dissection invoked for a specific UDP port,
-- so get the udp dissector table and add our protocol to it
DissectorTable.get("udp.port"):add(65333, test_proto)
DissectorTable.get("udp.port"):add(65346, test_proto)

print("test_proto dissector registered")
