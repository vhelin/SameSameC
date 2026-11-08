"""Independent application models for the portable Z80 comparison corpus."""

from collections import deque
from itertools import combinations, groupby


APPLICATION_CASES = {
    "calculationSpriteSceneWorkload": 12,
    "calculationTrackerWorkload": 12,
    "calculationNavigationWorkload": 12,
}


def application_inputs(name, case):
    seeds = (0, 1, 32767, 32768, 65535, 0x1234, 0xa55a, 255, 65280, 0x4321, 0x5555, 0xbeef)
    if name not in APPLICATION_CASES or not 0 <= case < APPLICATION_CASES[name]:
        raise ValueError(f"Unknown application case: {name}/{case}")
    seed = seeds[case]
    initial = bytearray((index * 37 + case * 53 + (index * index >> 3)) & 255 for index in range(512))
    if name == "calculationSpriteSceneWorkload":
        count = (0, 1, 2, 4, 6, 3, 6, 1, 4, 2, 5, 6)[case]
        expected, result = sprite_scene(seed, count, initial)
    elif name == "calculationTrackerWorkload":
        count = (0, 1, 4, 8, 16, 24, 2, 12, 20, 3, 16, 24)[case]
        if case == 1:
            for index in range(128):
                initial[384 + index] = (15 if index & 1 else 0) ^ ((seed + index * 11) & 255)
        expected, result = tracker(seed, count, initial)
    else:
        count = (0, 1, 1, 2, 3, 1, 2, 3, 1, 2, 3, 3)[case]
        if case % 4 in (1, 2, 3):
            initial[320:] = bytes([0 if case % 4 == 2 else 1]) * 192
        if case % 4 == 3:
            for row in range(2, 11, 2):
                gate = 14 if row % 4 == 0 else 1
                for column in range(1, 15):
                    if column != gate:
                        initial[320 + row * 16 + column] = 0
        expected, result = navigation(seed, count, initial)
    return [seed, count, 0xc100], initial, expected, result


def sprite_scene(seed, count, initial):
    output = bytearray(initial)
    terrain = [
        int(index // 16 in (0, 11) or index % 16 in (0, 15)
            or (seed + index * 13 + initial[400 + (index & 63)]) % 16 == 0)
        for index in range(192)
    ]
    actors = [
        {"x": (((seed + index * 3) & 65535) % 14 + 1) * 256,
         "y": (((seed >> 4) + index * 5) % 10 + 1) * 256,
         "direction": (seed >> (index & 7)) & 3,
         "speed": (seed + index * 5) % 8 + 1, "animation": index % 4}
        for index in range(12)
    ]
    hits = [0] * 12
    collisions = 0
    for actor in actors:
        terrain[actor["y"] // 256 * 16 + actor["x"] // 256] = 0
    for frame in range(count + 1):
        if frame:
            for index, actor in enumerate(actors):
                actor["direction"] ^= initial[400 + ((frame - 1) * 12 + index) % 64] & 3
                for axis, flag, limit in (("x", 1, 3584), ("y", 2, 2560)):
                    delta = actor["speed"] * (-16 if actor["direction"] & flag else 16)
                    proposed = (actor[axis] + delta) & 65535
                    tile_x = proposed // 256 if axis == "x" else actor["x"] // 256
                    tile_y = proposed // 256 if axis == "y" else actor["y"] // 256
                    if not 256 <= proposed <= limit or terrain[tile_y * 16 + tile_x]:
                        actor["direction"] ^= flag
                    else:
                        actor[axis] = proposed
                actor["animation"] = (actor["animation"] + actor["speed"] // 2 + 1) % 4
            for left, right in combinations(range(12), 2):
                if abs(actors[left]["x"] - actors[right]["x"]) < 192 and abs(actors[left]["y"] - actors[right]["y"]) < 192:
                    for index in (left, right):
                        actors[index]["direction"] ^= 3
                        hits[index] = (hits[index] + 1) & 255
                    collisions += 1
        output[:192] = bytes(terrain)
        for index, actor in enumerate(actors):
            output[actor["y"] // 256 * 16 + actor["x"] // 256] = 16 + index * 4 + actor["animation"]
    output[192:204] = bytes(hits)
    for index, actor in enumerate(actors):
        output[224 + index * 4:228 + index * 4] = actor["x"].to_bytes(2, "little") + actor["y"].to_bytes(2, "little")
        output[272 + index] = actor["direction"]
        output[284 + index] = actor["animation"]
    output[296:300] = collisions.to_bytes(2, "little") + bytes((count, 12))
    checksum = seed
    for value in output[:300]:
        checksum = (checksum * 17 + value) & 65535
    return output, (checksum + collisions) & 65535


def tracker(seed, count, initial):
    periods = (856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453)
    pattern = bytes(initial[384 + index] ^ ((seed + index * 11) & 255) for index in range(128))
    voices = []
    for channel in range(4):
        period = periods[((seed + channel) & 65535) % 12] >> ((seed >> channel) & 3)
        voices.append({"period": period, "target": period, "phase": 0, "volume": 0,
                       "gate": 0, "effect": 0, "arpeggio": 0})
    output = bytearray(initial)
    output[192:320] = pattern
    for tick in range(count):
        increments = []
        for channel, voice in enumerate(voices):
            if tick % 4 == 0:
                index = ((tick // 4) % 16) * 8 + channel * 2
                event, command = pattern[index:index + 2]
                note = event & 15
                if note < 12:
                    voice["target"] = periods[note] >> (event >> 6)
                voice.update(effect=(command >> 4) & 3, volume=command & 15,
                             gate=int(not (event & 32) and note < 12))
                if voice["effect"] != 1:
                    voice["period"] = voice["target"]
            if voice["effect"] == 1:
                difference = voice["target"] - voice["period"]
                voice["period"] += max(-2, min(2, difference))
            elif voice["effect"] == 2:
                voice["arpeggio"] = (voice["arpeggio"] + 1) % 3
                voice["period"] = voice["target"]
                if voice["arpeggio"]:
                    voice["period"] -= voice["target"] >> (4 - voice["arpeggio"])
            elif voice["effect"] == 3:
                voice["period"] = voice["target"] + min(tick % 8, 7 - tick % 8)
            if voice["gate"]:
                voice["volume"] = min(15, voice["volume"] + int(tick % 2 == 0))
            else:
                voice["volume"] = max(0, voice["volume"] - 1)
            increments.append(8192 // voice["period"] + 1)
        for sample in range(4):
            mix = 0
            for voice, increment in zip(voices, increments):
                voice["phase"] = (voice["phase"] + increment) & 65535
                wave = (voice["phase"] // 8) % 32
                mix += min(wave, 31 - wave) * voice["volume"]
            output[tick * 4 + sample] = (mix >> 2) & 255
    for channel, voice in enumerate(voices):
        offset = 128 + channel * 8
        output[offset:offset + 8] = (
            voice["period"].to_bytes(2, "little") + voice["phase"].to_bytes(2, "little")
            + bytes(voice[field] for field in ("volume", "gate", "effect", "arpeggio")))
    output[160:162] = bytes((count, 4))
    checksum = seed
    for value in output[:320]:
        checksum = (checksum * 33 + value) & 65535
    return output, checksum


def navigation(seed, count, initial):
    output = bytearray(initial)
    terrain = [index // 16 in (0, 11) or index % 16 in (0, 15) or initial[320 + index] % 8 == 0
               for index in range(192)]
    output[:192] = bytes(240 if blocked else 0 for blocked in terrain)
    output[192:288] = b"\xff" * 96
    output[288:320] = bytes(32)
    commands = []
    for job in range(count):
        start = 17 + ((seed + job * 37) & 65535) % 10 * 16 + ((seed >> 4) + job * 3) % 14
        goal = 17 + ((seed >> 8) + job * 53) % 10 * 16 + ((seed >> 2) + job * 5 + 7) % 14
        terrain[start] = terrain[goal] = False
        distances = {start: 0}
        parents = {}
        frontier = deque([start])
        while frontier:
            position = frontier.popleft()
            if position == goal:
                break
            for delta in (-1, 1, -16, 16):
                adjacent = position + delta
                if not 0 <= adjacent < 192 or (abs(delta) == 1 and adjacent // 16 != position // 16):
                    continue
                if not terrain[adjacent] and adjacent not in distances:
                    parents[adjacent] = position
                    distances[adjacent] = distances[position] + 1
                    frontier.append(adjacent)
        path = []
        if goal in distances:
            cursor = goal
            while cursor != start:
                path.append(cursor)
                cursor = parents[cursor]
            path.reverse()
        directions = [{1: 0, -1: 1, 16: 2, -16: 3}[right - left]
                      for left, right in zip([start, *path], path)]
        for direction, group in groupby(directions):
            length = len(list(group))
            while length:
                take = min(length, 63)
                commands.append(direction * 64 + take)
                length -= take
        commands.append(0)
        commands = commands[:96]
        output[:192] = bytes(240 if terrain[index] else
                             (distances[index] + job * 7) % 128 if index in distances else 255
                             for index in range(192))
        distance = distances.get(goal, 65535)
        offset = 288 + job * 8
        output[offset:offset + 8] = bytes((start, goal)) + distance.to_bytes(2, "little") + len(distances).to_bytes(2, "little") + bytes((len(path), len(commands)))
    output[192:192 + len(commands)] = bytes(commands)
    output[319] = len(commands)
    checksum = seed
    for value in output[:320]:
        checksum = (checksum * 17 + value) & 65535
    return output, checksum
