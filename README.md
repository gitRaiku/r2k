# r2k

A simple romaji to kana input method for both wayland and Xorg based on the [JMDict dictionary](http://jmdict.org/).

![r2k](https://github.com/gitRaiku/r2k/blob/master/resources/r2k.png?raw=true)

## Configuration
Since this project was inspired by suckless' [dmenu](https://tools.suckless.org/dmenu/) and their philosophy all configuration is done through the ``src/config.h`` file. In there you can also disable support for Xorg or Wayland typing. Rendering however is currently only handled in Xorg.

## Build and installation

```sh
make # For simply building the project
sudo make install # For also installing the binaries
```

## Running

To run simply start the daemon ``r2kd`` and connect to it using ``r2k``.

## Usage

After running ``r2k`` you will be promted to type in romaji, giving you the corresponding kanji/gana writings.

You can cycle through the options by using ``space`` and ``tab``. Select an option using ``enter`` and exit by using ``Ctrl-c``.

You can separate characters such as んえ from ね by using a backslash eg. ``n\e``.

You can add a ー anywhere in a string with a dash ``-``.

You can also write accented vowels by writing the accent and then the vowel, eg. ``:o`` -> ö.

Certain other characters are also available by starting with a ``.`` followed by brackets, tildas or punctuation marks. Some exceptions being that ``..`` becomes a full width space and ``.M``turns into a music note ``♪``. You can also type ß with a capital s ``S``.

## Inspiration

This project was started due to the lack of a better alternative, and was heavily inspired by suckless' 
