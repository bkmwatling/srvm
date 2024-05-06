##
# Symbolic Regular Expression with counters Virtual Machine
#
# @author Brendan Watling
# @file bru.c
# @version 0.1

# compiler flags
DEBUG     := -ggdb -gdwarf-4
OPTIMISE  := -O0
WARNING   := -Wall -Wextra -Wno-variadic-macros \
             -Wno-overlength-strings -pedantic
EXTRA     := -std=c11
STC_FLAGS := -DSTC_UTF_DISABLE_SV
CFLAGS    := $(DEBUG) $(OPTIMISE) $(WARNING) $(EXTRA) $(STC_FLAGS)
DFLAGS    ?= # -DDEBUG

# commands
CC        := clang
RM        := rm -f
COMPILE   := $(CC) $(CFLAGS) $(DFLAGS)

# directories
SRCDIR    := src
BINDIR    := bin
STCDIR    := $(SRCDIR)/stc
REDIR     := $(SRCDIR)/re
FADIR     := $(SRCDIR)/fa
VMDIR     := $(SRCDIR)/vm

# files
BRU_EXE   := bru
EXE       := $(BRU_EXE)

BRU_SRC   := $(SRCDIR)/$(BRU_EXE).c
EXE_SRC   := $(BRU_SRC)

FATP_SRC  := slice.c string_view.c vec.c
UTIL_SRC  := argparser.c utf.c
STC_SRC   := $(addprefix $(STCDIR)/fatp/, $(FATP_SRC)) \
			 $(addprefix $(STCDIR)/util/, $(UTIL_SRC))

RE_SRC    := $(wildcard $(REDIR)/*.c)
FA_SRC    := $(wildcard $(FADIR)/*.c) \
             $(wildcard $(FADIR)/constructions/*.c) \
             $(wildcard $(FADIR)/transformers/*.c)
VM_SRC    := $(wildcard $(VMDIR)/*.c) \
		     $(wildcard $(VMDIR)/thread_managers/*.c)

SRC       := $(filter-out $(EXE_SRC), $(wildcard $(SRCDIR)/*.c)) \
		     $(STC_SRC) $(RE_SRC) $(FA_SRC) $(VM_SRC)
OBJ       := $(SRC:.c=.o)

### RULES ######################################################################

# executables

$(BRU_EXE): $(BRU_SRC) $(OBJ) | $(BINDIR)
	$(COMPILE) -o $(BINDIR)/$@ $^

# units

%.o: %.c
	$(COMPILE) -c -o $@ $<

# BINDIR

$(BINDIR):
	mkdir -p $(BINDIR)

### PHONY TARGETS ##############################################################

all: $(EXE)

clean: cleanobj cleanbin

cleanobj:
	$(RM) $(OBJ)

cleanbin:
	$(RM) $(addprefix $(BINDIR)/, $(EXE))

.PHONY: all clean cleanobj cleanbin

# end
