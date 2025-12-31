# Makefile for presto.c
# Pure C implementation of presto behavioral data analyzer
#
# Targets:
#   make          - Build presto binary (default)
#   make clean    - Remove build artifacts (obj/, bin/)
#   make test     - Run basic test (requires test data)
#   make check    - Quick compile check
#
# Test programs (compile manually):
#   gcc -o test_iterator tests/test_iterator.c obj/bhv2.o -lm
#   gcc -o debug_vars tests/debug_vars.c obj/bhv2.o -lm

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
LDFLAGS = -lm

# Cairo for plotting (presto only)
CAIRO_CFLAGS = $(shell pkg-config --cflags cairo 2>/dev/null)
CAIRO_LDFLAGS = $(shell pkg-config --libs cairo 2>/dev/null)

SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files
BHV2_SRC = $(SRCDIR)/bhv2.c
PRESTO_SRC = $(SRCDIR)/presto_main.c $(SRCDIR)/skip.c $(SRCDIR)/macros.c $(SRCDIR)/plot.c

# Object files
BHV2_OBJ = $(OBJDIR)/bhv2.o
PRESTO_OBJ = $(OBJDIR)/presto_main.o $(OBJDIR)/skip.o $(OBJDIR)/macros.o $(OBJDIR)/plot.o

# Targets
PRESTO = $(BINDIR)/presto

.PHONY: all clean test presto

all: presto

presto: $(PRESTO)

$(PRESTO): $(BHV2_OBJ) $(PRESTO_OBJ) | $(BINDIR)
	$(CC) -o $@ $^ $(LDFLAGS) $(CAIRO_LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Plot needs Cairo
$(OBJDIR)/plot.o: $(SRCDIR)/plot.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CAIRO_CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Test with a real BHV2 file
test: presto
	@echo "Testing presto..."
	@if [ -f /data/limbs/data/bhv2/*.bhv2 ]; then \
		$(PRESTO) -XE0 /data/limbs/data/bhv2/*.bhv2; \
	else \
		echo "No test files found"; \
	fi

# Quick compile check for bhv2.c only
check: $(BHV2_OBJ)
	@echo "bhv2.c compiles successfully"
