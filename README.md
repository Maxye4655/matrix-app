# matrix

Terminal matrix rain with more options. Like cmatrix on steroids.

## Install

```sh
make && sudo make install
```

Or run directly:

```sh
./matrix
```

## Options

| Flag            | Description                                                                                     |
| --------------- | ----------------------------------------------------------------------------------------------- |
| `-c, --color`   | Color scheme: `green`, `red`, `blue`, `cyan`, `white`, `yellow`, `magenta`, `rainbow`, `random` |
| `-s, --speed`   | Fall speed 1-10 (default: 5)                                                                    |
| `-d, --density` | Drop density 1-10 (default: 5)                                                                  |
| `-C, --charset` | Character set: `japanese`, `ascii`, `hex`, `binary`                                             |
| `-p, --pattern` | Fall pattern: `matrix`, `rain`, `snow`, `fire`, `stream`                                        |
| `-b, --bold`    | Enable bold text                                                                                |
| `-f, --fade`    | Enable fade effect                                                                              |
| `--custom`      | Custom character string                                                                         |

## Keys

| Key       | Action              |
| --------- | ------------------- |
| `q`       | Quit                |
| `Space`   | Pause/resume        |
| `r`       | Reset               |
| `+` / `-` | Speed up/down       |
| `h`       | Toggle help overlay |

## Examples

```sh
matrix -c rainbow -p stream
matrix -c white -p snow -s 3
matrix -c green -p matrix -C hex
matrix -c random -s 8 -d 10
matrix --custom "👾🌈💥"
```

## Patterns

- **matrix** — classic falling code with bright head and fading tail
- **rain** — faster, shorter drops
- **snow** — single characters drifting side to side
- **fire** — short ember-like drops
- **stream** — dense continuous downpour

Requires ncursesw.
