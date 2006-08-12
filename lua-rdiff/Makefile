#
# lua-rdiff
# (c) 2006 Javier Guerra G.
# $Id: Makefile,v 1.1.1.2 2006-08-12 16:33:34 jguerra Exp $
#
 
# Compilation parameters
CC = gcc
CWARNS = -Wall -pedantic \
        -Waggregate-return \
        -Wcast-align \
        -Wmissing-prototypes \
        -Wstrict-prototypes \
        -Wnested-externs \
        -Wpointer-arith \
        -Wshadow \
        -Wwrite-strings


CFLAGS = $(CONFIG) $(CWARNS) -ansi -g -O2 -I/usr/include/lua5.1


lua_rdiff.so : lua_rdiff.o
	ld -o lua_rdiff.so -shared lua_rdiff.o -lrsync


