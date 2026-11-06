-- Original SM1 disc in PCSX-Redux: start in the Chase Venom level, leave Spider-Man
-- standing so Venom gets away, and capture the Game Over screen. Every 50 vblanks a
-- screenshot, and every 250 a burst of eight consecutive ones.
--
-- Run with tools/redux/run.sh gameover.lua; output goes to private/work/redux/gameover/.
local OUT = 'Z:/Programming/SpideyPS1Recomp/private/work/redux/' .. "gameover" .. '/'
local ev = io.open(OUT .. 'events.txt', 'w')
local mem = PCSX.getMemPtr()

local function off(a) return bit.band(a, 0x1FFFFF) end
local function u32(a) return ffi.cast('uint32_t*', mem + off(a))[0] end
local function u16(a) return ffi.cast('uint16_t*', mem + off(a))[0] end
local function i32(a) return ffi.cast('int32_t*', mem + off(a))[0] end
local function w32(a, v) ffi.cast('uint32_t*', mem + off(a))[0] = v end
local function cstr(a, n)
  local s = {}
  for i = 0, n - 1 do
    local b = mem[off(a + i)]
    if b == 0 then break end
    s[#s + 1] = string.char(b)
  end
  return table.concat(s)
end
local function ram(a) return a >= 0x80000000 and a < 0x80200000 end
local function note(...) ev:write(string.format(...), '\n'); ev:flush() end

local vs, levelAt, exitAt = 0, nil, nil
local presses = { {120, 'START'}, {420, 'CROSS'}, {600, 'CROSS'}, {780, 'CROSS'}, {960, 'CROSS'} }
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local held = {}

local function finish(code)
  ev:close()
  PCSX.quit(code)
end

local function shot(tag)
  local ss = PCSX.GPU.takeScreenShot()
  local f = Support.File.open(OUT .. string.format('%s_%dx%d_%d.raw', tag, tonumber(ss.width),
    tonumber(ss.height), tonumber(ss.bpp)), 'TRUNCATE')
  f:writeMoveSlice(ss.data); f:close()
end

local function findVenom()
  local a = u32(0x800B5234)
  for _ = 1, 1024 do
    if not ram(a) then return nil end
    if u16(a + 0x34) == 0x139 then return a end
    a = u32(a + 0x1C)
  end
end

local selected = false
local function tick()
  vs = vs + 1
  if not selected and not _G.bpDesc and vs >= (TITLE_AT or 3780) then armDesc() end
  if selected and not levelAt and ram(u32(0x800B5268)) and findVenom() then
    levelAt = vs; note('chase level live at vs=%d', vs)
  end
  local t = vs - (TITLE_AT or 3780)
  for _, pr in ipairs(presses) do
    if t == pr[1] then pad.setOverride(B[pr[2]]); held[pr[2]] = vs + 12 end
  end
  for name, untilVs in pairs(held) do
    if vs >= untilVs then pad.clearOverride(B[name]); held[name] = nil end
  end
  if levelAt then
    local l = vs - levelAt
    if l >= 1000 and (l % 50 == 0 or l % 250 < 8) then shot(string.format('l%05d', l)) end
    if l >= 4500 then finish(0) end
  end
  if vs > 60000 then note('gave up at vs=%d', vs); finish(4) end
end

local function guard(f)
  return function(...)
    local ok, err = pcall(f, ...)
    if not ok then note('error: %s', tostring(err)); finish(3) end
    return ok
  end
end

_G.goVsync = PCSX.Events.createEventListener('GPU::Vsync', guard(tick))

-- Level select: the same descriptor swap as the port's SPIDEY_LEVEL, at the game's
-- own lookup (80018800). Armed before the menu presses; removed once it has run.
function armDesc()
  note('descriptor breakpoint armed at vs=%d', vs)
  _G.bpDesc = PCSX.addBreakpoint(0x80018800, 'Exec', 4, 'desc', guard(function()
    local a0 = PCSX.getRegisters().GPR.n.a0
    if selected or a0 ~= 0x800A568C then return end
    local cur = cstr(a0, 16):lower()
    if not cur:match('^l%d+a%d+_t$') then return end
    if cur ~= 'l5a1_t' then
      for i = 1, 6 do mem[off(a0 + i - 1)] = string.byte('l5a1_t', i) end
      mem[off(a0 + 6)] = 0
    end
    note('descriptor %s -> l5a1_t at vs=%d', cur, vs)
    selected = true
    _G.bpDesc:remove()
  end))
end

note('armed')
