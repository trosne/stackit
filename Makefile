
SELF_DIR := $(dir $(lastword $(MAKEFILE_LIST)))


all:
	gcc -o test main.c stackexchange.c httpclient/httpclient.c -std=gnu99 -ljansson -I$(SELF_DIR)httpclient -lz -ggdb
