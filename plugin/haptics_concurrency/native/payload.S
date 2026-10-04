.section .rodata
.balign 16
.global concurrency_dex
.hidden concurrency_dex
concurrency_dex:
.incbin CONCURRENCY_DEX_FILE
.global concurrency_dex_end
.hidden concurrency_dex_end
concurrency_dex_end:
.section .note.GNU-stack,"",%progbits
