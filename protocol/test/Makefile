CC = gcc
CFLAGS = -Wall -Wextra -g
OUTDIR = _out
TARGET = $(OUTDIR)/pwar_router_test

SRCS = pwar_router_test.c ../pwar_router.c
INCLUDES = -I..

CHECK_CFLAGS = $(shell pkg-config --cflags check)
CHECK_LIBS = $(shell pkg-config --libs check)

all: $(TARGET)

$(OUTDIR):
	mkdir -p $@

$(TARGET): $(SRCS) | $(OUTDIR)
	@echo "Compiling: $@"
	$(CC) $(CFLAGS) $(INCLUDES) $(CHECK_CFLAGS) -o $@ $(SRCS) $(CHECK_LIBS)

clean:
	rm -rf $(OUTDIR)

.PHONY: all clean
