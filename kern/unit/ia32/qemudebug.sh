#! /bin/sh

qemu-system-i386 -s -S -soundhw ac97,sb16 -cdrom cd.iso -m 2048
#qemu-system-i386 -s -S -vga std -cdrom cd.iso -m 2048

