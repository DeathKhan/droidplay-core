# Divinity: Original Sin 2 - Definitive Edition (`010027400CDC6000`)

Update 1.0.3.1 (v196608) over the base game, SDK 8.2.0. Larian Studios' Divinity Engine (`FW3.5/Gold/Stable` in its asserts' paths), NVN, Wwise sound, RakNet 15.03.26. Modules `main`, `subsdk0` and `sdk`: `subsdk0` and `sdk` are one build (`3ae34e68…`), the SDK twice. The RomFS is 48 files, 10.74 GB: Larian's LSPK packages (`Data/*.pak`, version 13), the voice and localization packages beside them.

Status: brought up on Android, on an Ayn Odin 2 (Adreno 740, Android 13). The title, the main menu, a new game (Classic mode), character creation, the opening cinematic and the first area (the Merryweather's hold, Magister Siwan's first dialogue) in play, at the game's 30 fps. Not yet played further than that; see [Open](#open).

## What it needs

| Setting | Where | Why |
|---|---|---|
| `net_loopback on` | [`game.tsv`](game.tsv), `hle_loopback.c` | single player is a RakNet server and a client in one process; without UDP on 127.0.0.1 a new game waits forever at the loading screen. See [New game](#new-game) |
| `modules main subsdk0 sdk`, `mem_reserve 0x140000000`, `mem_alias 0x60000000`, `mem_stack 0x10000000`, `app_pool 0xCD500000`, `sys_resource 0x400000`, `depth_mode zero` | `game.tsv` | the provisional profile's values from the survey and the Mac's heap check (a 2.96 GB heap); kept as they were |
| LSPK packages read by the shader split | `dm/gfx/tools/shader_split.py` `lspk_shaders` | the shaders are in the `.bshd` files of `Data/*.pak`: 11,578 of them, where a plain scan found `main`'s 73 and every frame was black ([shaders](../../../docs/reference/shaders.md)) |

## New game

- **Resolved: New Game waited at the loading screen forever** (2026-11-10). The loading screen drew and its tips changed at 30 fps, with an empty progress bar, on the Odin, the Pad 3 and the Mac alike. The last files opened were the story's: `story_generated.div.osi` looked for in every mod and not found, then `Data/Patch1_Gold.pak`.
  - Every thread was idle or polling (`files/threads_now` on Android). The client's `GameThread` (`main+0x341350`) ran its frame handshake with the render thread, waiting 1 ms at a time on a light event (`main+0x39f40`); the `ServerWorker thread` ticked its own loop (`main+0x4f0ca0`) about 25 times a second.
  - The game state names are in two tables, the client's `ecl::EGameState` at `main+0x29d6dc0` (0 Unknown … 23 StartLoading, 25 StartServer) and the server's `esv::EGameState` at `main+0x2a170c0` (8 LoadSession, 12 Sync, 14 Running). Probes on the swap logs (`DM_PROBE=main:53fe0c,main:d17478` on the Mac, x4 and x5 the from and to names) showed the server go Init → LoadModule → LoadSession and stay there, and the client wait in StartServer.
  - LoadSession (`esv::GameStateLoadSession`, vtable `main+0x2a1c558`) runs its load on a thread, `ServerLoadSession` (`main+0xdb8220`, thread wrapper `main+0x37c90`), and its update (`main+0xdba8a0`) waits for the task's status 3. The thread ended normally (`Server Thread ended: ServerLoadSession`, status 3 set at `main+0x380d0`), so the load itself was fine.
  - The cause was the network: when the server starts, its RakNet peer (`main+0x1adc06c`, from `RakPeer.cpp`) calls `nn::socket::Socket(AF_INET, SOCK_DGRAM, 0)` twice, and the host failed every socket call. A Switch with no network still has loopback, which is how the game plays offline there.
  - Fix: `net_loopback` ([Loopback UDP needs no network](../../../docs/reference/services.md#loopback-udp-needs-no-network)). Both state machines then go on to Running: the server LoadSession → LoadLevel → Sync → Running, the client InitNetwork → InitConnection → … → SwapLevel → PrepareRunning → Running. Character creation loads 11 s after Begin on the Odin.
  - The peer opens one socket and sends one datagram, 4 bytes to itself, as it binds; the client and the server talk in process, not over UDP. Two RakNet threads run from then on: `Raknet#0` waits in `RecvFrom`, `Raknet#1` on a condition variable.
- **The route there:** Press A at the title; a Steam cross-save question comes on a fresh save (B, No: the answer is saved); Story; Single player; a difficulty (Classic is the default); Begin. `DM_KEYS=32:A,36:B,39:A,42:A,45:A,48:A,51:A` reaches the Begin press on the Odin from a launch with the save that answered the question (B at the main menu does nothing).

## Character creation

- Draws right, at 30 fps: GPU 7.5 ms a frame, 676 draws (Odin).
- **Its first tutorial box takes a held A.** A press of 150 ms (`DM_KEYS`, or adb's gamepad events) or 0.5 s does nothing there: the box stays, and no other button does anything either. A held for 1 s (`A~1.0`) closes it, and short presses work normally after that (Right changes the origin, B opens the main menu question). Seen the same in handheld and in console mode; the game reads player 1's state every frame there (`GetNpadState`, `main+0xdccc4`). Not looked into further.
- Start Game is Plus, then A on the confirmation.

## First area

- The opening cinematic (painted, narrated) plays through, then the Merryweather's hold with Magister Siwan's first dialogue. 30 fps (frame time p50 33.3 ms, p95 35.2), GPU 6.1 ms a frame, 331 draws, 1.5 GB resident in the replay (Odin).
- `DM_KEYS=32:A,36:B,39:A,42:A,45:A,48:A,51:A,70:A~1.0,76:PLUS,82:A` reaches it in about 2 minutes 30 s; the `tohold` route below does the same from a fresh save.
- Quick Save (Plus, then A) writes `Save0000/Save0000.lsv` and its thumbnail; Continue loads it again (`hold` below). The game autosaves on its own a few seconds into play.

- Walked by hand (2026-11-10, adb joystick events): the left stick moves the character with the camera following; the stairs at the hold's west end lead up a level to a doorway blocked by a toppled crate, the tutorial's Move puzzle (X on the crate opens the Action Menu: Pick Up, Open, Move, Attack, Examine; Pick Up clears it). The ladder in the room below asks whether to leave the Tutorial Deck; Yes loads the next level in about 25 s: the cabin deck, where Magister Siwan's dialogue moves the quest on (narrator lines, ambient remarks). 30 fps there too: frame time p50 33.3 / p95 33.8 ms, GPU 8.7 ms, 798 draws, submit 13.8 ms of CPU a frame. Not yet past its corridor to the top deck and its first fight. Holding the left stick in (L3) for 1 s on a tutorial box hides tutorials for the profile.
- Sound plays: Wwise mixes into AudioOut #0 (6 channels, `main+0x1bd3050`), which carries signal throughout (`DM_AUDIO_DUMP`); the audio renderer the game also opens (`main+0x162c988`) is nearly silent (436 of 24,000 frames), which is expected of it here.

## Routes

[`bench_android.tsv`](bench_android.tsv), for `scripts/bench_android.py`:

- `tohold`: a fresh save through New Game, character creation, the opening cinematic and Magister Siwan's first lines, then Quick Save in the hold at about 205 s. With `--keep-save <game folder>/saves/hold` it makes the save `hold` starts from (`Story - QuickSave_0`, 3.7 MB).
- `hold`: Continue from that save (in the hold 6 s after the press), a short walk, then idling; measured from 96 s for 30 s. Odin 2, warm-up and 2 runs (2026-11-10): 30.0 fps, frame p50 33.3 / p95 33.9 / p99 34.2 ms, GPU 5.9 ms, PSS 1.38 GB, the same in all three.

## Frame rate

- 30 fps, the game's own: it asks for present interval 2 (`WindowInitialize swap chain 2 textures, interval 2`), at 1600x900 cropped to 1280x720 in handheld mode.

## Open

- On a cold pipeline cache, 25 draws of a new game's first minutes are left out while their pipelines build in the background (`[vk] draws not made: … pipeline 25`, `pipeline built in the background … draws left out meanwhile`); with the cache warm, `hold` leaves out none.
- `[nvn] [RED] SetRenderEnableConditional: the host draws anyway for now`, from the title on.
- Further into the game (Fort Joy, the first fights, saving and loading) is not played yet.
