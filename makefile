CC=gcc
CFLAGS=-I.
INCLUDEFLAGS=-I/opt/vc/include
LIBFLAGS=-L/opt/vc/lib -lEGL -lGLESv2 -lbcm_host

triangle: triangle.c
	$(CC) -Wall $(INCLUDEFLAGS) -o triangle.bin triangle.c $(LIBFLAGS)
