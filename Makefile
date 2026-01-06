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
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -I$(SRCDIR)
LDFLAGS = -lm

# Cairo for plotting (presto only)
CAIRO_CFLAGS = $(shell pkg-config --cflags cairo 2>/dev/null)
CAIRO_LDFLAGS = $(shell pkg-config --libs cairo 2>/dev/null)

SRCDIR = src
OBJDIR = obj
BINDIR = bin
MACRODIR = $(SRCDIR)/macros

# Source files
BHV2_SRC = $(SRCDIR)/bhv2.c
ML_TRIAL_SRC = $(SRCDIR)/ml_trial.c
PRESTO_SRC = $(SRCDIR)/main.c $(SRCDIR)/skip.c $(SRCDIR)/macros.c

# Macro implementation files (in src/macros/)
MACRO_SRC = $(MACRODIR)/count.c \
            $(MACRODIR)/behavior.c \
            $(MACRODIR)/errors.c \
            $(MACRODIR)/scenes.c \
            $(MACRODIR)/analog.c \
            $(MACRODIR)/errorcounts.c \
            $(MACRODIR)/plot.c

# Object files
BHV2_OBJ = $(OBJDIR)/bhv2.o
ML_TRIAL_OBJ = $(OBJDIR)/ml_trial.o
PRESTO_OBJ = $(OBJDIR)/main.o $(OBJDIR)/skip.o $(OBJDIR)/macros.o
MACRO_OBJ = $(OBJDIR)/macro_count.o \
            $(OBJDIR)/macro_behavior.o \
            $(OBJDIR)/macro_errors.o \
            $(OBJDIR)/macro_scenes.o \
            $(OBJDIR)/macro_analog.o \
            $(OBJDIR)/macro_errorcounts.o \
            $(OBJDIR)/macro_plot.o

# Targets
PRESTO = $(BINDIR)/presto

.PHONY: all clean test presto

all: presto

presto: $(PRESTO)

$(PRESTO): $(BHV2_OBJ) $(ML_TRIAL_OBJ) $(PRESTO_OBJ) $(MACRO_OBJ) | $(BINDIR)
	$(CC) -o $@ $^ $(LDFLAGS) $(CAIRO_LDFLAGS)

# Core source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Macro implementation files (prefix with macro_ to avoid conflicts)
$(OBJDIR)/macro_count.o: $(MACRODIR)/count.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/macro_behavior.o: $(MACRODIR)/behavior.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/macro_errors.o: $(MACRODIR)/errors.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/macro_scenes.o: $(MACRODIR)/scenes.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/macro_analog.o: $(MACRODIR)/analog.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/macro_errorcounts.o: $(MACRODIR)/errorcounts.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Plot needs Cairo
$(OBJDIR)/macro_plot.o: $(MACRODIR)/plot.c | $(OBJDIR)
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
