# CLoTemp

A minimal CLI tool that displays the current temperature for your location, written in C.

Uses [Open-Meteo](https://open-meteo.com/) for weather data and [addr.se](https://addr.se/) for IP-based geolocation.

## Dependencies

- [libcurl](https://curl.se/libcurl/)
- [cJSON](https://github.com/DaveGamble/cJSON) (bundled in `lib/`)

## Build

```sh
make
```

The binary is output to `bin/clotemp`.

## Usage

First, initialize your location (detected from your IP):

```sh
clotemp init
```

This writes your coordinates to `~/.clotemp`.

Then, get the current temperature:

```sh
clotemp
```

```sh
1970-01-01T00:00 // 8.2°C
```

### Flags

| Command | Description              |
|---------|--------------------------|
| `-v`    | Print version            |
| `init`  | Initialize your location |
