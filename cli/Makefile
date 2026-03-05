TARGET_CLI  = mcfw 		 	# CLI


INCLUDE = -I../dependencies

CC = gcc
CFLAGS_CLI   = -Wall -Wextra -Werror -std=c17 -m64 -MMD -MP -g $(INCLUDE) -O2 -Wno-maybe-uninitialized

LIBS = -L../dependencies -l:crefl/libcrefl.a


OUT_ENV 	= ../app
OUT_OBJ 	= ../build/cli/obj

#verbose flag
ifeq ($(V),1)
  Q :=
else
  Q := @
endif


# ------ CLI OBJECTS ---------------
CLI_SRC = .
CLI_C := $(shell find $(CLI_SRC) -name "*.c" ! -path "$(CLI_SRC)/templates/*")
CLI_O := $(patsubst $(CLI_SRC)/%.c,$(OUT_OBJ)/%.o,$(CLI_C))

# -------------- CREFL STUFF -------------------
CREFL_FOLDER	= $(OUT_ENV)/.crefl
CRELF_FILE  = $(CREFL_FOLDER)/cli_types.crefl
PY_GEN      = python3 ../dependencies/crefl/crefl_gen
PY_INPUT    = $(shell find $(CLI_SRC) -name "*.h") $(shell find $(CLI_SRC) -name "*.c")
CFLAGS_CLI += -DCLI_CREFL=\"$(CRELF_FILE)\"

BIN_CLI  := $(OUT_ENV)/$(TARGET_CLI)

DEPS := $(CLI_O:.o=.d)


# ------------ COLORS ------------------
WHITE  = \033[0;39m
GREEN  = \033[0;92m
PURPLE = \033[0;95m
CYAN   = \033[0;96m
RED    = \033[0;91m
YELLOW = \033[0;33m

all: $(BIN_CLI)

# === COMPILE ===
$(OUT_OBJ)/%.o: $(CLI_SRC)/%.c | $(OUT_OBJ)
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS_CLI) -c $< -o $@
	@echo "$(GREEN)(+) $@.$(WHITE)"

# == LINK ==
$(BIN_CLI): $(CRELF_FILE) $(CLI_O) | $(OUT_ENV)
	$(Q)$(CC) $(CFLAGS_CLI) -o $@ $(CLI_O) $(LIBS)
	@echo "$(PURPLE)(+) $@.$(WHITE)"

$(CRELF_FILE): $(PY_INPUT) | $(CREFL_FOLDER) $(OUT_ENV)/.host
	$(Q)$(PY_GEN) -i $(PY_INPUT) -o $(CRELF_FILE)

$(OUT_ENV) $(CREFL_FOLDER) $(OUT_ENV)./host $(OUT_OBJ):
	@echo "$(CYAN)(+) $@.$(WHITE)"
	@mkdir -p $@

clean:
	@rm -f $(BIN_CLI) && echo "$(RED)(-) $(BIN_CLI)."
	@rm -rf $(OUT_OBJ) && echo "(-) $(OUT_OBJ).$(WHITE)"
	@rm -f $(CRELF_FILE) && echo "$(RED)(-) $(CRELF_FILE).$(WHITE)"


-include $(DEPS)
.PHONY: all clean
