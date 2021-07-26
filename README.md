# Bomber

`Bomber` is a popular multiplayer clone of [Bomberman](https://en.wikipedia.org/wiki/Bomberman) from David Ashley written for the `X11` windowing system. This version has been adapted to also work with `MacOS` sound and offers several other enhancements, e.g., score counting, extra bonuses, ... `Bomber` is written in `C`. David Ashley's original code is licensed under the GPL and the message accompanying his initial release can be found in  [DASH.txt](DASH.txt). `Bomber` will run on Linux and MacOS, where the former requires `OSS` support for sound, and the latter the [XQuartz](https://www.xquartz.org) `X11` window manager installed.

## Usage

`Bomber` is compiled with:

```shell
make
```

This results in a binary executable called `bomber` and the game is started with:

```shell
./bomber
```

For the ability to play LAN multiplayer games, the hostname or IP address of a matcher should be provided as the first command line argument, for example:

```shell
./bomber 192.168.20.100
```

The matcher needed for LAN multiplayer games is included and compiled with:

```shell
make daemon
```

This results in a binary executable called `matcherd`. When not already running, `matcherd` is automatically started by `Bomber`, or it can be started manually with:

```shell
./matcherd &
```

See [matcherd/README.md](matcherd/README.md) for additional information and configuring the matcher.

## Notes

1. On MacOS XCode and the developer tools needs to be installed.
2. On Ubuntu the `osspd-alsa` package is needed for sound.

## BSD-3 License

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
