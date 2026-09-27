# Request flow

How a button press on the web page reaches an output pin. Example: the button
"on" of output 3 is the link `/?3=on`.

```mermaid
flowchart TD
    A["Browser: tap 'on' at Output 3"] --> B["GET /?3=on HTTP/1.1 + headers"]
    B --> C["loop(): client = server.available()"]
    C --> D["Read_Request(): store request line in request_line[]"]
    D --> E["Read_Request(): read over headers until the empty line"]
    E --> F{"complete within 2 s?"}
    F -- no --> Z["Clear_Buffer(), client.stop()"]
    F -- yes --> G{"Is_Page_Request():<br/>'GET / ' or 'GET /?'"}
    G -- "no (e.g. /favicon.ico)" --> N["404 Not Found, nothing switched"]
    G -- yes --> P["Parse_Request(): digit, 2 chars later 'o', then 'n'/'f'"]
    P --> H["'3=on' found -> digitalWrite(PIN_OUTPUT_3, HIGH)"]
    P --> Q["'all=0' found -> All_Outputs_Low()<br/>(No_SD and SD only)"]
    H --> S["Send_Page(): 200 + page<br/>(built in the sketch, or index.htm from SD)"]
    Q --> S
    N --> Z
    S --> Z
```

## Reading the request

`Read_Request()` is a small state machine with two states:

| State | What happens |
|---|---|
| `READ_LINE` | every byte except `\r` goes into `request_line[]` (at most 99 chars, the last place stays 0); `\n` switches to `SKIP_HEADERS` |
| `SKIP_HEADERS` | header lines are read and dropped; an empty line (`\r\n` right after `\r\n`) ends the request -> `true` |

It returns `false` if the client closes the connection first or if 2 s pass.
Only the request line is kept, so header text (for example a `Referer` that
contains `?3=on`) can never switch an output.

## The parser

The parser does no real URL parsing. It walks the request line and matches the
pattern directly: an output number followed two characters later by `'o'` and
then `'n'` (on) or `'f'` (off). `?3=on` contains `3=on`, `?3=off` contains
`3=of`. Several parameters in one request (`/?3=on&4=off`) are all applied.

`Webserver_No_SD` and `Webserver_SD` also look for the text `all=0`
(`/?all=0`) and then set every output LOW.

## The answer

| Request | Answer |
|---|---|
| `/` or `/?...` | `200 OK`, the page (`Webserver_SD`: `503` if the card or `index.htm` is missing) |
| anything else | `404 Not Found`, short text |

Every answer has `Connection: close`; the sketch closes the connection with
`client.stop()` after sending.
