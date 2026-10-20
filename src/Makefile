TARGET_HOST = .mcfw_server	# Server host binary


VENDOR_DIR = vendor 
INCLUDE_DIR = include

# --------- OUT FOLDERS ------------------
OUT_ENV      	= ../app/.mcfw
OUT_ENV_HOST	= $(OUT_ENV)/bin
OUT_ENV_INC   	= $(OUT_ENV)/include
OUT_ENV_LIB		= $(OUT_ENV)/lib
CREFL_FOLDER	= $(OUT_ENV)/crefl
OUT_OBJ 		= ../build/obj/host


CC = gcc
CFLAGS_HOST_D = -Wall -Wextra -Werror -std=c17 -I$(INCLUDE_DIR) -I$(OUT_ENV_INC) -I$(VENDOR_DIR) -m64 -MMD -MP -g 
CFLAGS_HOST_R = -Wall -Wextra -Werror -std=c17 -I$(INCLUDE_DIR) -I$(OUT_ENV_INC) -I$(VENDOR_DIR) -m64 -MMD -MP -g -O2
CFLAGS = $(CFLAGS_HOST_D)

LIBS = -L$(OUT_ENV_LIB) -Wl,--whole-archive -l:libcrefl.a \
	 -l:libasclib.a -Wl,--no-whole-archive \
	-rdynamic -ldl -lpthread




# ------ HOST OBJECTS ---------------
HOST_SRC = .
HOST_C := $(shell find $(HOST_SRC) -name "*.c")
HOST_O := $(patsubst $(HOST_SRC)/%.c,$(OUT_OBJ)/%.o,$(HOST_C))

# -------------- CREFL STUFF -------------------
CRELF_FILE  	= $(CREFL_FOLDER)/host_types.crefl
PY_GEN      	= python3 $(OUT_ENV_LIB)/crefl_gen
PY_INPUT   	 	= $(shell find $(HOST_SRC) -name "*.h") $(shell find $(HOST_SRC) -name "*.c")
CFLAGS_HOST_D 	+= -DHOST_CREFL=\"$(CRELF_FILE)\"
CFLAGS_HOST_R 	+= -DHOST_CREFL=\"$(CRELF_FILE)\"

DEPS := $(HOST_O:.o=.d)

BIN_HOST := $(OUT_ENV_HOST)/$(TARGET_HOST)

CLI := $(OUT_ENV)/mcfw

# ------------ COLORS ------------------
WHITE  = \033[0;39m
GREEN  = \033[0;92m
PURPLE = \033[0;95m
CYAN   = \033[0;96m
RED    = \033[0;91m
YELLOW = \033[0;33m

#verbose flag
ifeq ($(V),1)
  Q :=
else
  Q := @
endif

$(OUT_OBJ)/gperf/str_to_content_type.o: CFLAGS += -Wno-missing-field-initializers

all: $(BIN_HOST)
all:
	@mkdir -p $(OUT_ENV_INC)/mcfw
	@cp -r $(INCLUDE_DIR)/public/* $(OUT_ENV_INC)/mcfw

# == COMPILE ==== 
$(OUT_OBJ)/%.o: $(HOST_SRC)/%.c | $(OUT_OBJ)
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
	@echo "$(GREEN)(+) $@.$(WHITE)"

# == LINK ==
$(BIN_HOST): $(CRELF_FILE) $(HOST_O) | $(OUT_ENV_HOST)
	$(Q)$(CC) $(CFLAGS) -o $@ $(HOST_O) $(LIBS)
	@echo "$(PURPLE)(+) $@.$(WHITE)"

$(CRELF_FILE): $(PY_INPUT) | $(CREFL_FOLDER) $(OUT_ENV_HOST)
	$(Q)$(PY_GEN) -i $(PY_INPUT) -o $(CRELF_FILE)

$(OUT_ENV) $(CREFL_FOLDER) $(OUT_ENV_HOST) $(OUT_OBJ):
	@mkdir -p $@ && echo "$(CYAN)(+) $@.$(WHITE)"

-include $(DEPS)
clean:
	@rm -f $(BIN_HOST) && echo "$(RED)(-) $(BIN_HOST)."
	@rm -rf $(OUT_OBJ) && echo "(-) $(OUT_OBJ)."
	@rm -f $(CRELF_FILE) && echo "(-) $(CRELF_FILE).$(WHITE)"

run: CFLAGS = $(CFLAGS_HOST_D) 
run: all
	@echo "$(YELLOW)Running ====> $(BIN_HOST).$(WHITE)"
	@echo "  "
	@./$(CLI) run --set-pwd $(OUT_ENV)



.PHONY: all clean run




 