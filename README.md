# AuDecoder, v1.0
---------------

How to copy an 8-bit Atari floppy disk to your pc-platform without any cables, etc?
You do have a disk drive, but no sio2pc, sio2sd, sdrive, sio2usb, etc, don't you?

Just retype the basic program disk2snd.bas into atari powered without any keys pressed. You may boot to dos and save this program on the disk for latter use.
The data lines are summed up, so if you make an error during retyping, the program will tell you. After 6-7 seconds after run, it will ask for three numbers: start sector (you answer 1), end sector (you answer for example 1000, but 720 will work too unless in ED) and choose the sector length. The very first three sectors are always 128 bytes long so you do not have to bother, you enter the regular sector length (for SD you enter 1, for DD you enter 2). In case of errors, sectors will not be encoded to audio. In case of mistake, you can always press reset and run program again (no need to retype it).

disk2snd is as simple as it could be, so:
it encodes sectors one by one - not the fastest way, but simplest and shortest; the sector reading and encoding is done in machine language.

Of course you do not have to retype first remarked lines.

How to use.
-----------

I presume you have connected your atari to pc using audio cable? Ok. one cable needed:)
And pressed "record" button in audio app before, right? (I prefer Audacity).

Then run disk2snd and record those beeps to audio app, save the file as WAV, and load it in AuDecoder, which will reverse all the operation resulting in freshly made disk image at the PC side. Only 8/16 bits, mono/stereo WAV files  supported (command line version supports mono only for now).

Tips:
DD/SS disk takes 4.5 min of audio to encode.
When errors encountered, you may encode only bad encoded sectors and read again into AuDecoder (first big WAV, next small WAV). Then save the file as ATR or XFD (headerless) disk image. This may happen when poor audio quality is recorded. Remember, record at 44100Hz! either 48000 HZ or lower than 44100 will not work!
On the top right you have three buttons: showing basic program disk2snd, clearing log and resetting program to initial values.

End words
---------

Decoder is very fast in general. It decodes whole disk image in less than 2 seconds in most cases. In case of major problems (no data decoded at standard parameters) the algorithm switches to brute-force - it sets threshold to incrementing values until all sectors are succesfuly decoded or the threshold exceeds maximum value.
The signal does not have to be strong; the most important is that it is 10% volume or louder, not clipped, not filtered. At 16 bits loudness has to be louder than 1% :)
Every data block (shshshsht) contains sector number, sector length and checksum, and of course sector data.
The bitrate is 7800 (only ones) to 15600 (only zeroes) bits/sec. so in average there is 11 kbits (no start and stop bits), ca 1.3 kB/sec 

Because I cannot get Lazarus working on my MacBooks, I have translated main.pas to audecode.c with help of AI, so conversion and testing took a few hours. The result sits in cmdline catalog, simple run ./compile.sh and you get working binary.

Happy copying!

And last but not least: although you may use this software as you wish, I am not responsible for any damages made with this software. Your risk, your choice. However, the code does not contain any destructive code, only "save to disk".

This is completely free software.

Jakub Husak
