# Wiring notes

The hardware is an Arduino (ATmega328 class board, e.g. Uno) with an official
**Arduino Ethernet shield** stacked on top. The shield carries a WIZnet **W5100**
Ethernet controller and a **micro-SD card slot**, both on the SPI bus.

## Pins used by the shield (not free for outputs)

| Pin | Used by | Note |
|---|---|---|
| 10 | W5100 Ethernet chip select | occupied whenever Ethernet is used |
| 4  | SD-card chip select | occupied whenever the SD slot is used |
| 11, 12, 13 | SPI (MOSI, MISO, SCK) | shared bus for W5100 and SD |

Because chip-select 4 belongs to the SD card and chip-select 10 belongs to the
W5100, those pins must not be reused as switching outputs. `Webserver_SD` sets
pin 10 HIGH (W5100 not selected) before it starts the SD card, so the two chips
do not both answer on the shared bus.

## Outputs (relay / LED)

Each output pin drives a load — a relay module or an LED — that represents one
switchable "light". The web page turns them on and off.

| Sketch | Output pins | Why |
|---|---|---|
| `Webserver_Basic`       | 3, 4 | no SD used, so pin 4 is free |
| `Webserver_No_SD`   | 3, 4 | no SD used, so pin 4 is free |
| `Webserver_SD`      | **2, 3** | SD is used, so pin 4 (SD CS) is not free — pin 2 is used instead |

In the two non-SD sketches pin 4 drives an output, so leave the SD slot empty
there (a card would see pin 4 as its chip select).

This shift from pins 3/4 to pins 2/3 in the SD variant is the key wiring
consequence of adding the SD card.

Drive relays/LEDs through a suitable driver stage (transistor or relay module
with its own driver); do not switch inductive loads directly from a GPIO pin. Mains voltage on the relay
contacts belongs in a closed enclosure and may only be wired by a qualified person.
Active-high in this firmware: `HIGH` = output on.
