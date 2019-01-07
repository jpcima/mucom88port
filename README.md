# mucom88port
OpenMucom88 port

## Introduction

This is a port of OpenMucom88, from [original](https://github.com/onitama/mucom88) Windows sources of the software by [onionsoftware](https://github.com/onitama/) and Yuzo Koshiro.  
This supports GNU/Linux, and possibly macOS and others.

## License

This derivative software is licensed, like the original work, under a CC BY-NC-SA 4.0 license.  
Please note: a non-commmercial usage restriction applies, therefore this is not considered free software
under the [definition](https://www.gnu.org/philosophy/free-sw.html.en) established by the Free Software Foundation.

## Build

You will need development packages for `libuv` and `libsamplerate`.  
On GNU/Linux, you also need one or more development packages for audio: `alsa`, `pulseaudio`, `jack`

```
git clone https://github.com/jpcima/mucom88port.git
cd mucom88port
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

After a successful build, the program will be available as `mucom88` in the build directory.

## Example usage

1. Compile a `muc` file of your choice.

```
mucom88 -c marb2.muc
```

The file `mucom88.mub` will be produced.

2. Play the result.

```
mucom88 mucom88.mub
```
