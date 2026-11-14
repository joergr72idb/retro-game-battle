; Startup code for SDCC programs on the CPC: sets up initialised data and calls main().
;   MEMORY &3FFF:LOAD"NAME":CALL &4000   main returns to BASIC
;   RUN"NAME"               works too, but RUN resets the firmware and the CPC restarts at the end

        .module crt0
        .globl  _main
        .globl  l__INITIALIZER, s__INITIALIZER, s__INITIALIZED     ; set by the linker

        .area   _HEADER (ABS)
        .org    0x4000
        jp      init

        ; the order of the areas = their order in memory
        .area   _HOME
        .area   _CODE
        .area   _INITIALIZER
        .area   _GSINIT
        .area   _GSFINAL
        .area   _DATA
        .area   _INITIALIZED
        .area   _BSEG
        .area   _BSS
        .area   _HEAP

        .area   _CODE
init:
        call    gsinit
        jp      _main               ; main's ret goes back to the caller

        .area   _GSINIT
gsinit::
        ld      bc, #l__INITIALIZER ; copy the initial values of global variables
        ld      a, b
        or      a, c
        jr      z, gsinit_next
        ld      de, #s__INITIALIZED
        ld      hl, #s__INITIALIZER
        ldir
gsinit_next:

        .area   _GSFINAL
        ret
