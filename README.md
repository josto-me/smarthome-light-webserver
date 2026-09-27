# Smart Home Light Webserver

[![DOI](https://img.shields.io/badge/DOI-10.5281%2Fzenodo.22991717-blue.svg)](https://doi.org/10.5281/zenodo.22991717) [![Build](https://github.com/josto-me/smarthome-light-webserver/actions/workflows/build.yml/badge.svg)](https://github.com/josto-me/smarthome-light-webserver/actions/workflows/build.yml) [![Code: Apache-2.0](https://img.shields.io/badge/code-Apache--2.0-blue.svg)](LICENSE) [![Docs: CC BY 4.0](https://img.shields.io/badge/docs-CC%20BY%204.0-lightgrey.svg)](LICENSE-CC-BY-4.0.txt) [![Cite](https://img.shields.io/badge/cite-CITATION.cff-green.svg)](CITATION.cff)

Ethernet-Lichtfernsteuerung mit Arduino, HTL-Schulprojekt.

## About

A small Arduino web server that switches outputs (relays or LEDs) from a web page
in the browser — a remote light switch ("Lichtfernsteuerung"). The firmware runs
on an Arduino with an Ethernet shield; a button on the page is a link such as `/?3=on`, and a
small char-array parser finds the output number and on/off in the request line
and switches the matching pin.

## Hardware

| | |
|---|---|
| Board | Arduino Uno or another ATmega328P board |
| Shield | Ethernet shield with WIZnet W5100 and micro-SD slot |
| Outputs | 2 digital outputs driving relays or LEDs ("lights") |
| Network | 10/100 Ethernet, HTTP server on port 80 |

Pin usage and why the SD variant uses different pins: see `docs/wiring.md`.

## Safety and disclaimer

This is a school project, not a certified product. The web server has **no login and no
encryption**: anyone who can reach it can switch the outputs. Run it only in a trusted local
network and never make it reachable from the internet (no port forwarding). Relays that
switch mains voltage must be wired by a qualified person according to the local regulations.
No warranty, see the licenses.

## How it works

1. `loop()` takes a client from `EthernetServer::available()`.
2. `Read_Request()` stores the request line (`GET /?3=on HTTP/1.1`) in a
   100-byte char buffer and then reads over the header lines until the empty
   line. It gives up after 2 s or when the client closes the connection.
3. `Is_Page_Request()` accepts only `/` and `/?...`. Anything else (for example
   `/favicon.ico`) gets a `404` and switches nothing.
4. `Parse_Request()` scans the buffer for an output number
   followed two characters later by `o`, then `n` (on) or `f` (off).
5. `Send_Page()` answers with the page, then the buffer is cleared and the
   connection closed.

The full path is drawn in `docs/request-flow.md`.

## The pages

All three pages are mobile-friendly (`<meta name="viewport">`, system font,
large tap targets). The buttons are plain links with GET parameters, so no
JavaScript or forms are needed.

| Sketch | Page |
|---|---|
| `Webserver_Basic` | minimal page: one line per output, `lit`/`dark` state, `[on]` `[off]` links |
| `Webserver_No_SD` | one card per output with a coloured state badge and on/off buttons, an "everything dark" button (`/?all=0`); all text from flash (`F()`) |
| `Webserver_SD` | `index.htm` from the SD card: a grid of cards for outputs 2 and 3 plus "everything dark"; static, so it does not show the state |

## Contents

```
firmware/Webserver_Basic/        basic variant: inline page, outputs 3/4
firmware/Webserver_No_SD/        page from flash (F()), outputs 3/4, ?all=0
firmware/Webserver_SD/           page from SD card, outputs 2/3, ?all=0
  └─ index.htm                   page served from the SD card
docs/request-flow.md             GET /?3=on -> parser -> pin (Mermaid diagram)
docs/wiring.md                   shield CS pins 4/10, outputs, why pins 2/3 on SD
docs/iterations.md               comparison of the three sketches
ATTRIBUTION.md                   acknowledgements
LICENSE, NOTICE                  Apache License 2.0 (code)
LICENSE-CC-BY-4.0.txt            CC BY 4.0 (docs)
CITATION.cff                     citation metadata
```

## Build

Open a sketch in the **Arduino IDE** (or use `arduino-cli`) and upload it to the
board. Before uploading, set the network values at the top of the sketch (MAC /
IP / gateway / DNS / subnet). The committed values are placeholders
(`02:00:00:00:00:01`, `192.168.1.177`). For `Webserver_SD`, copy `index.htm`
to the root of a FAT-formatted micro-SD card.

The GitHub Actions workflow compiles all three sketches for `arduino:avr:uno`.
The sketches are not tested on hardware.

## Dependencies

Not vendored; they come with the Arduino IDE or its library manager. Their licenses apply to them and to compiled binaries that contain them.

| Component | License |
|---|---|
| Arduino AVR core (incl. `SPI`) | LGPL-2.1-or-later (core); `SPI`: GPL-2.0 or LGPL-2.1 |
| `Ethernet` library 2.x | mixed: MIT, Apache-2.0 and LGPL-2.1 files |
| `SD` library (only `Webserver_SD`) | GPL-3.0 |

Because the `SD` library is GPL-3.0, a **compiled binary** of `Webserver_SD` that
you distribute falls under the GPL-3.0. The source code in this repository stays
Apache-2.0.

## Known limitations

- The SD page is static and cannot show the output state.
- No login and no encryption: anyone in the same network can switch the outputs.

## License

- Code (`firmware/`, including `index.htm`): **Apache License 2.0**, see
  [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE). All of it is the author's own work.
- Documentation (`docs/`, this README and the other `.md` files): **CC BY 4.0**,
  see [`LICENSE-CC-BY-4.0.txt`](LICENSE-CC-BY-4.0.txt).
- Dependencies keep their own licenses, see [Dependencies](#dependencies).

You may use, change and share everything, also commercially. When you pass it on or
publish something based on it, credit it as:

> Johannes Stockhammer, "Smart Home Light Webserver", version 1.0.0, Zenodo, https://doi.org/10.5281/zenodo.22991718

GitHub shows the same citation under "Cite this repository" (from [`CITATION.cff`](CITATION.cff)).

Arduino is a trademark of Arduino SA and WIZnet a trademark of WIZnet Co.; used
only to identify compatible hardware.

## Author

Johannes Stockhammer

Concept and hardware by Johannes Stockhammer. The firmware (a new implementation of the original school project) and the documentation were written with the help of AI tools and reviewed by the author.
