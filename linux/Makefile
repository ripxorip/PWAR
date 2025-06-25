CC = gcc
CFLAGS += -Iprotocol $(shell pkg-config --cflags libpipewire-0.3) -I../protocol -Wall
LDFLAGS = -lm $(shell pkg-config --libs libpipewire-0.3)
TARGET = pwarPipeWire
SRCS = pwarPipeWire.c
OBJS = $(addprefix $(OUTDIR)/, $(SRCS:.c=.o))
OUTDIR = _out
Q = @

# Add torture test target
TORTURE_TARGET = pwar_torture
TORTURE_SRC = torture.c
TORTURE_OBJ = $(OUTDIR)/torture.o

all: dir $(TARGET) $(TORTURE_TARGET)

dir:
	$(Q)mkdir -p $(OUTDIR)

$(TARGET): $(OBJS)
	$(Q)$(CC) $(CFLAGS) -o $(OUTDIR)/$(TARGET) $(OBJS) $(LDFLAGS)

$(TORTURE_TARGET): $(TORTURE_OBJ)
	$(Q)$(CC) $(CFLAGS) -o $(OUTDIR)/$(TORTURE_TARGET) $(TORTURE_OBJ) $(LDFLAGS)

$(OUTDIR)/%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/torture.o: torture.c
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(Q)rm -f $(OUTDIR)/*.o
	$(Q)rm -f $(OUTDIR)/$(TARGET)
	$(Q)rm -f $(OUTDIR)/$(TORTURE_TARGET)
	$(Q)rmdir $(OUTDIR)
