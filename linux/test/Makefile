PIPEWIRE_CFLAGS := $(shell pkg-config --cflags libpipewire-0.3)
PIPEWIRE_LIBS := $(shell pkg-config --libs libpipewire-0.3)

OUTDIR = ../_out

$(OUTDIR)/integration_test: integration_test.c
	mkdir -p $(OUTDIR)
	$(CC) $(PIPEWIRE_CFLAGS) -o $(OUTDIR)/integration_test integration_test.c $(PIPEWIRE_LIBS) -lm

all: $(OUTDIR)/integration_test

clean:
	rm -f $(OUTDIR)/integration_test

.PHONY: all clean
