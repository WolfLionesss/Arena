# JoA Arena

JoA Arena is a two-player turn-based game. One player
fields several figures on a shared board.

## License

Original code and original art are copyright Wolfgang Andreas Knapp,
all rights reserved. Third-party art, models, and
sound keep their own licenses. The full text is
[LICENSE.md](LICENSE.md). The same credits, for a
Steam build, are in [Credits.txt](Credits.txt).

## Engine

The project is Unreal Engine 5.8.1 from a source
build. `JoA_Arena.uproject` points at that build with
its engine association id. A launcher install of
another version will not open the project until the
association is retargeted.

The engine source is not part of this repository. Do
not upload it. Epic licenses the engine separately.
This game project is the part that can be published.

## Open the project

1. Install the same Unreal Engine 5.8.1 source build.
2. Generate project files for `JoA_Arena.uproject`.
3. Build the `JoA_Arena` editor target.
4. Open the project in that editor.

`SteamDevAppId=480` in `Config/DefaultEngine.ini` is
Valve's public Spacewar app. It is only for local
tests. Replace it with the real Steam AppID before a
release build.