TARGET_CLI  = mcfw 		 	# CLI


CC = gcc
CFLAGS_CLI   = -Wall -Wextra -Werror -std=c17 -m64 -MMD -MP -g

OUT_ENV 	= ../app
OUT_OBJ 	= ../build/cli/obj


# ------ CLI OBJECTS ---------------
CLI_SRC = .
CLI_C := $(shell find $(CLI_SRC) -name "*.c" ! -path "$(CLI_SRC)/templates/*")
CLI_O := $(patsubst $(CLI_SRC)/%.c,$(OUT_OBJ)/%.o,$(CLI_C))

BIN_CLI  := $(OUT_ENV)/$(TARGET_CLI)

DEPS := $(CLI_O:.o=d)
-include $(DEPS)


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
	$(CC) $(CFLAGS_CLI) -c $< -o $@
	@echo "$(GREEN)(+) $@.$(WHITE)"

# == LINK ==
$(BIN_CLI): $(CLI_O) | $(OUT_ENV)
	$(CC) $(CFLAGS_CLI) -o $@ $^
	@echo "$(PURPLE)(+) $@.$(WHITE)"



$(OUT_ENV) $(OUT_OBJ):
	@echo "$(CYAN)(+) $@.$(WHITE)"
	@mkdir -p $@

clean:
	@rm -f $(BIN_CLI) && echo "$(RED)(-) $(BIN_CLI)."
	@rm -rf $(OUT_OBJ) && echo "(-) $(OUT_OBJ).$(WHITE)"

.PHONY: all clean
