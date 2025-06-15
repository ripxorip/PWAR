CC = gcc
CFLAGS = -Wall -Wextra -g -lm
OUTDIR = _out
TARGET = $(OUTDIR)/pwar_router_test
TARGET_RCV = $(OUTDIR)/pwar_rcv_buffer_test
TARGET_SEND = $(OUTDIR)/pwar_send_buffer_test
TARGET_CHAIN = $(OUTDIR)/pwar_send_receive_chain_test

SRCS = pwar_router_test.c ../pwar_router.c
SRCS_RCV = pwar_rcv_buffer_test.c ../pwar_rcv_buffer.c
SRCS_SEND = pwar_send_buffer_test.c ../pwar_send_buffer.c
SRCS_CHAIN = pwar_send_receive_chain_test.c ../pwar_send_buffer.c ../pwar_router.c ../pwar_rcv_buffer.c
INCLUDES = -I..

CHECK_CFLAGS = $(shell pkg-config --cflags check)
CHECK_LIBS = $(shell pkg-config --libs check)

all: $(TARGET) $(TARGET_RCV) $(TARGET_SEND) $(TARGET_CHAIN)

$(OUTDIR):
	mkdir -p $@

$(TARGET): $(SRCS) | $(OUTDIR)
	@echo "Compiling: $@"
	$(CC) $(CFLAGS) $(INCLUDES) $(CHECK_CFLAGS) -o $@ $(SRCS) $(CHECK_LIBS)

$(TARGET_RCV): $(SRCS_RCV) | $(OUTDIR)
	@echo "Compiling: $@"
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SRCS_RCV) $(CHECK_LIBS)

$(TARGET_SEND): $(SRCS_SEND) | $(OUTDIR)
	@echo "Compiling: $@"
	$(CC) $(CFLAGS) $(INCLUDES) $(CHECK_CFLAGS) -o $@ $(SRCS_SEND) $(CHECK_LIBS)

$(TARGET_CHAIN): $(SRCS_CHAIN) | $(OUTDIR)
	@echo "Compiling: $@"
	$(CC) $(CFLAGS) $(INCLUDES) $(CHECK_CFLAGS) -o $@ $(SRCS_CHAIN) $(CHECK_LIBS)

run: all
	@echo "Running all tests..."
	@$(TARGET)
	@$(TARGET_RCV)
	@$(TARGET_SEND)
	@$(TARGET_CHAIN)

clean:
	rm -rf $(OUTDIR)

.PHONY: all clean
