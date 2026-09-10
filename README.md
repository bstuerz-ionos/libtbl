# libtbl

A small C11 library for printing arrays of C structs as **tables**, **JSON**, **CSV**
or **XML**.

You describe your struct once as a list of columns; libtbl derives everything else
(offsets, sizes, widths, alignment, formatting) from that description. It is aimed at
CLI tools that want `ls -l` / `lsblk -o` / `ip -j link` style output without hand-rolling
`printf` width juggling for every command.

Only libc is required.

## Features

- Automatic column widths, measured by rendering each cell through a counting stream
- Unicode-aware widths via `mbrtowc`/`wcwidth` — box drawing and CJK line up correctly
- Per-column alignment, optional sum/total row with a separator rule
- ANSI colorization driven by a per-value callback
- "Human" vs. "raw" rendering (`15 MB` vs. `15728640`)
- Runtime column selection by name, with `+`/`-` prefixes (`--columns=name,size`)
- Self-describing: the schema itself is a table, so `--help`-style column listings are free
- No runtime registration — table descriptors are `const` data in `.rodata`

## Example

```c
#include "libtbl.h"
#include "bytes.h"

struct netif {
	char			*name;
	enum netif_state	 state;
	int			 mtu;
	struct {
		long			 rx_packets;
		unsigned long long	 rx_bytes;
	} rx;
};

#define COLUMNS_NETIF(X)                                                                              \
	X(netif, name,          "Interface",  "Network Interface",          TBLC_DEFAULT,          NULL)  \
	X(netif, mtu,           "MTU",        "Maximum Transmission Size",  TBLC_DEFAULT,          &ops_mtu)   \
	X(netif, rx.rx_packets, "RX Packets", "Number of received packets", TBLC_SUM|TBLC_ARIGHT,  NULL)  \
	X(netif, rx.rx_bytes,   "RX Bytes",   "Number of received bytes",   TBLC_SUM|TBLC_ARIGHT,  &ops_bytes) \
	X(netif, state,         "State",      "up/down",                    TBLC_DEFAULT,          &ops_netif_state)

TBL_DEFINE_STATIC (netif, COLUMNS_NETIF);

tbl_print_table (stdout, netif, nifs, TBL_ARRAY_SIZE (nifs), NULL, TBL_DEFAULT);
```

```
Interface   MTU RX Packets RX Bytes State
─────────────────────────────────────────
eth0       1500      12543    15 MB up
wlan0      1500       3821     2 MB down
lo        65536     118244    42 MB down
tun0       9000        256     1 KB up
                ────────── ────────
                    134864    59 MB
```

## API

Everything is in `libtbl.h`. The `str` argument of every macro below is the bare
struct name that was passed to `TBL_DEFINE`, *not* a value or a pointer.

### Defining a table

| | |
| --- | --- |
| `TBL_DEFINE(name, f)` | Expand the X-macro column list `f` into `struct tbl_cd[]` + `struct tbl_td` for `struct name` |
| `TBL_DEFINE_STATIC(name, f)` | Same, but `static` |
| `TBL_NCOLS(name)` | Number of columns, usable as an array size |
| `TBL_ARRAY_SIZE(a)` | Generic array length helper |

Each `X` entry is `X(struct_name, member, header, help, flags, ops)`. `member` may be a
nested path such as `rx.rx_bytes`. Passing `NULL` for `ops` infers the vtable via
`_Generic` for the integer types and `char *`/`const char *`; any other type resolves to
`_tbl_ops_undefined`, which is deliberately never defined — misuse becomes a **link
error** rather than a runtime surprise.

### Printing

| | |
| --- | --- |
| `tbl_print(file, str, rows, nrows, csel, tflags, fmt)` | Dispatch on `enum tbl_format` |
| `tbl_print_table(file, str, rows, nrows, csel, tflags)` | Aligned table with header and sum row |
| `tbl_print_elem(file, str, row, csel, tflags)` | One record, printed vertically |
| `tbl_print_json(file, str, rows, nrows, csel)` | Complete `{ "rows": [ ... ] }` object |
| `tbl_print_csv(file, str, rows, nrows, csel, sep)` | Header line + rows |
| `tbl_print_xml(file, str, rows, nrows, csel)` | `<rows><name>...</name></rows>` |
| `tbl_print_descr(file, str, csel, tflags)` | The schema of `str` as a table |

`rows` must be a contiguous array of `nrows` elements; there is no iterator interface.
All return `int` (0 on success).

### Column selection

`csel` is a `size_t` array of column indices terminated by `TBL_CNUL`, or `NULL` for
"all columns in declaration order".

```c
size_t csel[TBL_NCOLS (netif) + 1];
tbl_select_cols (netif, "name,rx.rx_bytes", ',', csel);  /* or "+mtu", "-name", NULL for all */
tbl_print_table (stdout, netif, nifs, n, csel, TBL_DEFAULT);
```

`tbl_select_cols` returns -1 on an unknown name. `tbl_coli(str, field)` returns the
index of a single column and validates the field name at compile time.

### Enums

| Enum | Values |
| --- | --- |
| `enum tbl_format` | `TBLF_TABLE`, `TBLF_ELEMENT`, `TBLF_JSON`, `TBLF_CSV`, `TBLF_XML` |
| `enum tbl_flags` (`tflags`) | `TBL_DEFAULT`, `TBL_RAW`, `TBL_NOSUM`, `TBL_NOHEADER`, `TBL_NOCOLOR` |
| `enum tblc_flags` (`cd_flags`) | `TBLC_DEFAULT` (inherit from ops), `TBLC_SUM`, `TBLC_ALEFT`, `TBLC_ARIGHT` |
| `enum tbl_stringify_type` (`sflags`) | `TBLS_RAW`, `TBLS_HUMAN`, `TBLS_JSON` |
| `enum tbl_color` | `TBL_COLOR_RESET`, `..._BLACK` … `..._WHITE`, or'able with `TBL_COLOR_BOLD` / `TBL_COLOR_INTENSE` |

### Custom types

A custom column type is just a `struct tbl_type_ops`:

```c
struct tbl_type_ops {
	int	  to_deflags;				/* default tblc_flags */
	void	(*to_stringify)(FILE *, const void *, int);	/* value -> text, int is sflags */
	int	(*to_colorize)(const void *);		/* value -> enum tbl_color */
	void	(*to_add)(void *, const void *);	/* accumulator += value, for TBLC_SUM */
};
```

`to_stringify` must write the *same text width* during the measuring pass and the
printing pass; column widths depend on it. Handle `TBLS_JSON` if you use JSON output —
strings must quote themselves, and `tbl_write_json()` is exported for escaping.
`examples/bytes.h` is a ~45-line reference implementation rendering `unsigned long long`
as `B`/`KB`/`MB`/`GB`/`TB`.

## Building

```sh
make                        # static + shared library, then examples/
make check                  # builds and runs test/
make install PREFIX=/usr    # libtbl.a, libtbl.so*, libtbl.h, libtbl.pc
```

Link with:

```sh
cc -std=c11 myprog.c -ltbl
cc -std=c11 myprog.c $(pkg-config --cflags --libs libtbl)
```

Compile-time knobs:

- `-DUSE_ALLOCA=0` — use `calloc()`/`free()` instead of `alloca()` for scratch buffers
- `-DTBL4_COMPAT=0` — drop libtbl4 behaviour in CSV/XML (v4 stringifies with `TBLS_JSON`
  and uses `<columns>` as the XML row tag; without it, `TBLS_RAW` and the type name)

## Examples

Built by `make`, sources in `examples/`:

- `netif.c` — the tutorial: nested members, custom ops, colorization, every output
  format, every column-selection mode
- `find.c` — an `ls`/`tree` hybrid over a directory, `-d -s -r -c -j -x`
- `tree.c` — Unicode tree drawing inside a table cell, proving widths still align

## Requirements and caveats

- **C11 required** (`_Generic`, `_Static_assert`)
- Uses `fopencookie`, `wcwidth` and `alloca` — glibc, or a libc with the GNU extensions
- Call `setlocale (LC_CTYPE, "")` at startup, or widths fall back to `strlen()`
- **CSV and XML output are not escaped.** Separators, quotes and `&`/`<`/`>` in values
  will corrupt the output.
- **JSON validity depends on your ops** — see `to_stringify` above
- **Colors are unconditional ANSI.** There is no `isatty()` check; pass `TBL_NOCOLOR`
  when not writing to a terminal.
- Each cell is rendered 2–3 times (measure, then print). Fine for CLI-sized data, not
  for large streams.

## Contributors
- Grzegorz Prajsner <grzegorz.prajsner@ionos.com>
- Danil Kipnis <danil.kipnis@ionos.com>
- Supriti Singh <supriti.singh@ionos.com>
- Florian-Ewald Mueller <florian-ewald.mueller@ionos.com>
- Moritz Wagner <moritz.wagner@ionos.com>
- Vaishali Thakkar <vaishali.thakkar@ionos.com>
