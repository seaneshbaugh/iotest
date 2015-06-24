# IOTest

So I was wondering what the best buffer size for reading largish files was and the only real answer I could find was "test it yourself!" So I did just that. This program creates a 128 MB temporary file and then reads the file using different buffer sizes, starting at 512 bytes and then doubling up to 128 MB. When it's done it'll output the fastest average time so you know roughly what buffer size to use on your system.

Maybe I'll add more options later if I feel like it. Currently each buffer size gets 16 trials which probably isn't enough. And no, I haven't tested it on Linux yet.

## Building

Just run `make`.

## Running

After running make the executable file will be in the build directory.

    $ build/iotest
