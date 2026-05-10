## VGA_TOP Register Map (Byte Addressed, 32-bit Wide)

| Offset      | Register               | Description                                         | Valid Bits       | Value Range                                  | R/W |
|:-----------:|:----------------------:|:---------------------------------------------------:|:----------------:|:--------------------------------------------:|:---:|
| `0x00`      | `CTRL_REG`             | Control register (e.g. tilemap index, audio ctrl)   | [31:0]           | See bit field description below              |  W  |
| `0x04`      | `STATUS_REG`           | Current pixel column and row                        | [19:0]           | [19:10] col: 0–639<br>[9:0] row: 0–479        |  R  |
| `0x08–0x7F` | Reserved               | Reserved for future use                             | —                | —                                            | —   |
| `0x80–0xFF` | `SPRITE_ATTR_TABLE[n]` | Sprite attribute table (32 entries, 4 bytes each)   | [31:0]           | See format below                             |  W  |

---

### `CTRL_REG` Bit Field Description

| Bits     | Name         | Description                                                   |
|----------|--------------|---------------------------------------------------------------|
| [1:0]    | `tilemap_idx`| Tilemap index (2 bits): selects one of 4 tilemaps (0–3)       |
| [28:2]   | —            | Reserved                                                      |
| [30:29]  | `sfx_sel`    | Sound effect selector: <br>00 = None, 01/10/11 = 3 types       |
| [31]     | `bgm_en`     | Background music enable: <br>1 = On, 0 = Off                   |

---

### `SPRITE_ATTR_TABLE` Format (Each Entry = 4 Bytes)

Each entry at offset: `0x80 + (n × 4)`, where `n ∈ [0, 31]`

| Bits    | Field       | Description                          |
|---------|-------------|--------------------------------------|
| [31]    | `enable`    | 1 = visible, 0 = hidden               |
| [30]    | `flip`      | 1 = horizontally flipped              |
| [29:27] | Reserved    | Unused                               |
| [26:18] | `sprite_y`  | Vertical position (0–479)            |
| [17:8]  | `sprite_x`  | Horizontal position (0–639)          |
| [7:0]   | `frame_id`  | Sprite frame index (0–255)           |

---

### Notes

- All addresses are byte-aligned and 32-bit (4-byte) wide.
- Valid `SPRITE_ATTR_TABLE[n]` range: `n = 0 to 31` → offset `0x80` to `0xFC`
- Only `0x00`, `0x04`, and `0x80–0xFF` are valid; others are reserved.