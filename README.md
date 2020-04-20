# avr-size-plus
Alternative to avr-size for avr0/1 series.

Uses avr-readelf to provide the memory information needed. The executable file will need to be in the same folder as the toolchain provided avr-readelf. The only optional argument is `-d` to see debugging information and all avr-readelf output. The avr-readelf is called twice with first the `-S` option for section info, then `-s` for symbol info. The program was compiled and tested on a Linux pc that is based on Ubuntu 16.04 LTS. No compiler options were used, and the resulting binary is 64bit in my case (and is the executable in this repository).

Additional info can also be added in the future, such as a function list, object list, sorted by address or size, grouped by section, etc., to give an idea of what functions or objects are taking the most flash and ram.

### to compile

>$ gcc avr-size-plus.c -o avr-size-plus

if 14k is too large for you :) then you can also run-

>$ strip avr-size-plus

### example usage and output-

assuming the file `avr-readelf` from your toolchain is in this folder-

>$ ./avr-size-plus
```
usage:
  avr-size-plus [-d] /full/path/to/myapp.elf
  -d = optional debug output
```

>$ ./avr-size-plus /path/to/my.elf

```text
---FLASH-------------size--address---
available            8192
.text                3102 0x000000
.rodata                90 0x808C1E
used                 3192 [ 38%]
free                 5000 [ 62%]

 xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 xxxxx............................
 .................................

---RAM---------------size--address---
available             512
.data                   2 0x803E00
.bss                   88 0x803E02
.noinit                 0 0x000000
used                   90 [ 17%]
free                  422 [ 83%]

 xxxxxxxxxxxxxxxxx................
 .................................
 .................................

---EEPROM------------size--address---
available             160 [128/32]
.eeprom                 0 0x000000
.user_signatures        0 0x000000
used                    0 [  0%]
free                  160 [100%]
```
