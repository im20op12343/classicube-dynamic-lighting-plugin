# classicube-dynamic-lighting-plugin
Dynamic Lighting for classicube

i jst wanna say, FUCK NA2 AND ITS PLAYERS

Adds dynamic lighting for classicube.
When holding a block which has a light level or has fullbright, it will make a light around you.

## Compiling

### Setup
1) Create `ClassiCube.exe` by either:
    1) Compiling the game, see `Cross compiling for windows (64 bit)` in [main readme](/readme.md#cross-compiling-for-windows-64-bit)
    2) Downloading 64 bit ClassiCube from https://www.classicube.net/download/#dl-win
2) Install the `mingw-w64-tools` package (if it isn't already)
3) Generate the list of exported symbols from `ClassiCube.exe` by running:
    * `gendef ClassiCube.exe`
4) Create a linkable library from the exported symbols list by running: 
    * `x86_64-w64-mingw32-dlltool -d ClassiCube.def -l libClassiCube.a -D ClassiCube.exe`

### Compiling

`x86_64-w64-mingw32-gcc TestPlugin.c -o TestPlugin.dll -s -shared -L . -lClassiCube`

Then put `TestPlugin.dll` into your game's `plugins` folder. Done.

The only currect builds are for windows 64 bit.
The Releases were built on Windows 7 SP1
