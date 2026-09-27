# Three variants

The three sketches have the same structure (`Read_Request()`, `Is_Page_Request()`,
`Parse_Request()`, `Send_Page()`); they differ in where the page comes from and
what it can do.

| | `Webserver_Basic` | `Webserver_No_SD` | `Webserver_SD` |
|---|---|---|---|
| Page | built in the sketch, plain strings | built in the sketch, `F()` strings in flash | `index.htm` from the SD card |
| Page style | minimal text lines | cards with state badge and buttons | grid of cards, static |
| Shows state | yes (`lit`/`dark`) | yes (badge) | no |
| Output pins | 3, 4 | 3, 4 | 2, 3 (pin 4 = SD CS) |
| `?all=0` (everything dark) | no | yes | yes |
| Parser form | forward (`check_k+3`) | forward (`check_k+3`) | backward (`check_k-3`) |

## Reading order

1. `Webserver_Basic` — the smallest server that switches two pins
   from GET parameters and shows their state.
2. `Webserver_No_SD` — the same with a nicer page. All page text is in flash
   (`F()`), which keeps the 2 KB RAM of the ATmega328P free; adds `?all=0`.
3. `Webserver_SD` — the page moves out of the sketch onto the SD card, so it can
   be changed without uploading new firmware. That costs pin 4 (SD chip select),
   so the outputs move to pins 2 and 3. The page is a static file and cannot
   show the state.

All three use the same char-array parser, which avoids the Arduino `String` class.
