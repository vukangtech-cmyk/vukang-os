; =============================================================
;  VUKANG OS - kernel/assets.asm
;  Binary assets embedded into the kernel image:
;    console font (tools/mkfont.py) and boot splash layers
;    (tools/mksplash.py). Paths are relative to the repository root.
; =============================================================
section .rodata
align 16
global asset_font
asset_font:     incbin "assets/font/console.vkfont"
align 16
global asset_emblem
asset_emblem:   incbin "assets/splash/emblem.vkimg"
align 16
global asset_wordmark
asset_wordmark: incbin "assets/splash/wordmark.vkimg"
