TARGET_HOST = .mcfw_server	# Server host binary


INCLUDE = ../dependencies
CC = gcc
CFLAGS_HOST_D = -Wall -Wextra -Werror -std=c17 -I$(INCLUDE) -m64 -MMD -MP -g
CFLAGS_HOST_R = -Wall -Wextra -Werror -std=c17 -I$(INCLUDE) -m64 -MMD -MP -O2

CFLAGS = $(CFLAGS_HOST_R)

# --------- OUT FOLDERS ------------------
OUT_ENV      	= ../app
OUT_ENV_HOST	= ../app/.host
OUT_OBJ 		= ../build/obj/host

# ------ HOST OBJECTS ---------------
HOST_SRC = .
HOST_C := $(shell find $(HOST_SRC) -name "*.c")
HOST_O := $(patsubst $(HOST_SRC)/%.c,$(OUT_OBJ)/%.o,$(HOST_C))

DEPS := $(CLI_O:.o=d) $(HOST_O:.o=d)
-include $(DEPS)

BIN_HOST := $(OUT_ENV_HOST)/$(TARGET_HOST)

# ------------ COLORS ------------------
WHITE  = \033[0;39m
GREEN  = \033[0;92m
PURPLE = \033[0;95m
CYAN   = \033[0;96m
RED    = \033[0;91m
YELLOW = \033[0;33m


all: $(BIN_HOST)

# == COMPILE ==== 
$(OUT_OBJ)/%.o: $(HOST_SRC)/%.c | $(OUT_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "$(GREEN)(+) $@.$(WHITE)"

# == LINK ==
$(BIN_HOST): $(HOST_O) | $(OUT_ENV_HOST)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "$(PURPLE)(+) $@.$(WHITE)"

$(OUT_ENV_HOST) $(OUT_OBJ):
	@mkdir -p $@ && echo "$(CYAN)(+) $@.$(WHITE)"

clean:
	@rm -f $(BIN_HOST) && echo "$(RED)(-) $(BIN_HOST)."
	@rm -rf $(OUT_OBJ) && echo "(-) $(OUT_OBJ).$(WHITE)"

run: CFLAGS = $(CFLAGS_HOST_D) 
run: all
	@echo "$(YELLOW)Running ====> $(BIN_HOST).$(WHITE)"
	@echo "  "
	@./$(BIN_HOST) --set-dir $(OUT_ENV)



.PHONY: all clean run




 