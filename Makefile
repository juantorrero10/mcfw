# --------- OUT FOLDERS ------------------
OUT_ENV 		= app
BUILD 			= build
OUT_OBJ 		= build/obj

# ------------ COLORS ------------------
WHITE  = \033[0;39m
GREEN  = \033[0;92m
PURPLE = \033[0;95m
CYAN   = \033[0;96m
RED    = \033[0;91m
YELLOW = \033[0;33m

-include $(DEPS)


all: cli framework
	@echo "$(YELLOW)Created app enviroment at ==> $(OUT_ENV).$(WHITE)"
	@echo "   >>>   To start an application change dir to $(CYAN)./app$(WHITE) and run $(GREEN)./mcfw init$(WHITE)."


cli:
	@mkdir -p $(BUILD) && echo "$(CYAN)(+) $(BUILD).$(WHITE)" 
	@mkdir -p $(OUT_OBJ) && echo "$(CYAN)(+) $(OUT_OBJ).$(WHITE)"
	@$(MAKE) -C cli

framework:
	@mkdir -p $(BUILD) && echo "$(CYAN)(+) $(BUILD).$(WHITE)" 
	@mkdir -p $(OUT_OBJ) && echo "$(CYAN)(+) $(OUT_OBJ).$(WHITE)"
	@$(MAKE) -C src

clean:
	@$(MAKE) clean -C cli
	@rm -rf $(BUILD) && echo "$(RED)(-) $(BUILD).$(WHITE)"

run:
	@$(MAKE) run -C src


.PHONY: all cli clean run




 