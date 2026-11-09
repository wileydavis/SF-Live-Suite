-- SF SHOW and SF MIX: GLIDE RINGS. A knob that is travelling (SLOW, SLOW ALL) shows where its sound is now,
-- in green, until it arrives; then the ring is the knob's own again. Turn a travelling knob and the
-- ring shows your hand for a moment. The knobs themselves are the scene's plain CC knobs: if this
-- script stops, the rings show the knobs' values as before.
--
-- SF CONTROL reports the travelling knobs (E16 RINGS port), as SysEx:
--   F0 7D 53 46 01 {channel-1, number, value / 128, value % 128} ... F7   where the sound is (0-16383)
--   F0 7D 53 46 02 {channel-1, number} ... F7                              arrived
-- MAP (make_scene.py writes it from layout.py, one per scene): three characters per knob, the
-- channel - 1 as one hex digit and the number as two, "---" for a knob that never travels: knobs
-- 1-16 of page 1, then of page 2, ... page 12.
-- Lua 5.4 on the E16: integers only, no clock but system.update.
local MAP = "@MAP@"
local GREEN = @GREEN@
local HOLD = 24        -- updates (50 ms) without a report before a ring goes back: 1.2 s
local HAND = 16        -- updates a turned knob keeps showing your hand: 0.8 s
local held, hand, here, tick = {}, {}, {}, 0

-- The addresses of the knobs on page pg, channel * 128 + number (-1: none).
local function look(pg)
  for i = 1, 16 do
    local k = ((pg - 1) * 16 + i - 1) * 3 + 1
    local c = MAP:sub(k, k)
    here[i] = (c == "-") and -1 or (tonumber(c, 16) * 128 + tonumber(MAP:sub(k + 1, k + 2), 16))
  end
end

local function free(i)
  if held[i] then
    leds.reset(i)
    held[i] = nil
  end
end

function controller.onSysex(b)
  if b[2] ~= 0x7D or b[3] ~= 0x53 or b[4] ~= 0x46 then return end
  local kind = b[5]
  local step = (kind == 1) and 4 or 2
  for k = 6, #b - step, step do
    local a = b[k] * 128 + b[k + 1]
    for i = 1, 16 do
      if here[i] == a then
        if kind == 2 then
          free(i)
        elseif kind == 1 and (hand[i] or -1) < tick then
          leds.updateByIndex(i, b[k + 2] * 128 + b[k + 3], GREEN)
          held[i] = tick
        end
      end
    end
  end
end

function controller.onEncoderTurn(e)
  if e.increment ~= 0 and e.index then
    hand[e.index] = tick + HAND
    free(e.index)
  end
end

function system.update()
  tick = tick + 1
  for i = 1, 16 do
    if held[i] and tick - held[i] > HOLD then free(i) end
  end
end

function page.onPageChange(prev, curr)
  for i = 1, 16 do free(i) end
  hand = {}
  look(curr)
end

function page.onInit()
  for i = 1, 16 do leds.reset(i) end
  look(controller.getPage())
  system.setUpdateRate(50)
end
