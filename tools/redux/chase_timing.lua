-- Original SM1 disc in PCSX-Redux: start in the Chase Venom level, walk Spider-Man along
-- Venom's route the same way the port's SPIDEY_CHASE_FOLLOW fixture does, and log how
-- many console vblanks every main-loop update takes.
local OUT = 'Z:/Programming/SpideyPS1Recomp/private/work/redux/' .. (CHASE_OUT or 'chase') .. '/'
local log = io.open(OUT .. 'updates.csv', 'w')
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

local vs, titleAt, levelAt, released, exitAt = 0, nil, nil, false, nil
local presses = { {120, 'START'}, {420, 'CROSS'}, {600, 'CROSS'}, {780, 'CROSS'}, {960, 'CROSS'} }
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local held = {}

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

-- Follower: the port queues Venom's position every console tick and writes the one
-- from 20 ticks ago into the player.
local queue, qh, qt = {}, 1, 0
local regions = { 290, 291, 34, 39, 44 }
local nextPoints = { 16, 24, 35, 41, 50 }
local regionIndex, waitStart, pendingRegion = 1, nil, nil

local function follow()
  if not levelAt or released then return end
  local player, venom = u32(0x800B5268), findVenom()
  if not ram(player) or not venom then return end
  if regionIndex <= #regions and not pendingRegion then
    local task, script = u32(venom + 0x320), u32(venom + 0x31C)
    local waiting = ram(task) and ram(script) and u32(task) == 15 and u32(venom + 0x318) == 0
      and u16(script + 4) == nextPoints[regionIndex]
    if not waiting then waitStart = nil
    elseif not waitStart then waitStart = vs
    elseif vs - waitStart >= 60 then pendingRegion = regions[regionIndex]; regionIndex = regionIndex + 1; waitStart = nil end
  end
  qt = qt + 1; queue[qt] = { u32(venom + 4), u32(venom + 8), u32(venom + 12) }
  if qt - qh + 1 <= 20 then return end
  local p = queue[qh]; queue[qh] = nil; qh = qh + 1
  w32(player + 4, p[1]); w32(player + 8, p[2]); w32(player + 12, p[3])
end

local selected, lastGC = false, nil
local function poll()
  -- Level select: the port swaps the current descriptor name l1a1_t -> l5a1_t before
  -- the game looks it up. Polling once per vblank catches it while it is still l1a1_t.
  if not selected and not _G.bpDesc and vs >= (TITLE_AT or 3780) then armDesc() end
  if selected and not levelAt and ram(u32(0x800B5268)) and findVenom() then
    levelAt = vs; exitAt = vs + (CHASE_TICKS or 6000); note('chase level live at vs=%d', vs)
  end
  if levelAt and not released and regionIndex > #regions then
    local player = u32(0x800B5268)
    if ram(player) and u32(player + 0x1A8) ~= 0 then released = true; note('released: player script active at level+%d', vs - levelAt) end
  end
  -- One row per game update: the native game counter changes once per update.
  local gc = u32(0x800B4F38)
  if levelAt and gc ~= lastGC then
    local player, venom = u32(0x800B5268), findVenom()
    local px, pz, sc = 0, 0, 0
    if ram(player) then px, pz, sc = i32(player + 4) / 4096, i32(player + 12) / 4096, u32(player + 0x1A8) end
    local vx, vz = 0, 0
    if venom then vx, vz = i32(venom + 4) / 4096, i32(venom + 12) / 4096 end
    log:write(string.format('%d,%d,%.0f,%.0f,%.0f,%.0f,%d\n', vs - levelAt, gc, px, pz, vx, vz, sc))
  end
  lastGC = gc
end

local function tick()
  vs = vs + 1
  poll()
  if pendingRegion and not _G.bpLoop then armPulse() end
  do
    local t = vs - (TITLE_AT or 3780)
    for _, pr in ipairs(presses) do
      if t == pr[1] then pad.setOverride(B[pr[2]]); held[pr[2]] = vs + 12 end
    end
  end
  for name, untilVs in pairs(held) do
    if vs >= untilVs then pad.clearOverride(B[name]); held[name] = nil end
  end
  follow()
  if levelAt and (vs - levelAt) % 300 == 0 then shot(string.format('l%05d', vs - levelAt)) end
  if not levelAt and vs % 300 == 0 then shot(string.format('b%05d', vs)) end
  if exitAt and vs >= exitAt then log:close(); ev:close(); PCSX.quit(0) end
  if vs > 40000 then note('gave up at vs=%d', vs); log:close(); ev:close(); PCSX.quit(4) end
end

local function guard(f)
  return function(...)
    local ok, err = pcall(f, ...)
    if not ok then note('error: %s', tostring(err)); log:close(); ev:close(); PCSX.quit(3) end
    return ok
  end
end

_G.chaseVsync = PCSX.Events.createEventListener('GPU::Vsync', guard(tick))

-- Main loop, just after its second vblank wait: one row per game update. Region pulses
-- are run here by calling the original region-command routine, then restoring state.
-- Level select: the same descriptor swap as the port's SPIDEY_LEVEL, at the game's
-- own lookup. Armed only after the menu presses; removed once it has run.
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

local saved = nil
function armPulse()
_G.bpLoop = PCSX.addBreakpoint(0x8002C294, 'Exec', 4, 'loop', guard(function()
  local regs = PCSX.getRegisters()
  if saved then
    for i = 0, 33 do regs.GPR.r[i] = saved[i] end
    saved = nil
    _G.bpLoop:remove(); _G.bpLoop = nil
    return
  end
  if pendingRegion then
    saved = {}
    for i = 0, 33 do saved[i] = regs.GPR.r[i] end
    regs.GPR.n.a0 = pendingRegion
    regs.GPR.n.ra = 0x8002C294
    regs.GPR.n.sp = regs.GPR.n.sp - 0x100
    note('region pulse %d at vs=%d level+%d', pendingRegion, vs, vs - levelAt)
    pendingRegion = nil
    regs.pc = 0x8005BA58
  end
end))
end
log:write('level_tick,game_counter,player_x,player_z,venom_x,venom_z,player_script\n')
note('armed')
