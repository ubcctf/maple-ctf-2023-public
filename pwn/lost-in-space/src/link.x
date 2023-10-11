ENTRY(entry)
SECTIONS
{
  . = 0x10000;
  prog_start = .;
  . = . + SIZEOF_HEADERS;
  .text : {
    *(.text)
    *(.text.*)
  }
  .rodata : {
    *(.rodata)
    *(.rodata.*)
  }
  .data : {
    *(.data)
    *(.data.*)
  }
  load_end = .;
  .bss : {
    *(.bss)
    *(.bss.*)
  }
  prog_end = .;

  . = ALIGN(0x1000);
  .text.touser : {
    *(.touser)
  }

  . = ALIGN(0x1000);
 /* /DISCARD/ : { *(*) } */
}
