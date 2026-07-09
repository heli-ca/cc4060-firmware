# rule.mk - CC4060 Firmware Build Rules
# Included by apps/Makefile via: -include $(MAKE_RULE)
#
# CRITICAL: CWD = apps/ (build runs via `make -C apps`)
# All source paths in SRCS_C are relative to apps/
# All relative paths (../include_lib/, etc.) resolve from apps/

# ---- Include SDK compiler Makefile for CC, LD, CC_ARGS, etc. ----
# This defines the pi32-clang compiler path, flags, include paths, and linker settings
-include tools/compiler/Makefile.pi32_lto

# ---- Object files (compiled from SRCS_C defined in apps/Makefile) ----
OBJS = $(addprefix $(DIR_OUTPUT)/obj/, $(patsubst %.c,%.o, $(SRCS_C)))

# ---- Default target ----
all: pre_make $(OUTPUT_EXES)
	@echo "============================================"
	@echo " CC4060 firmware build complete"
	@echo " Output: $(OUTPUT_EXES)"
	@echo " Objects compiled: $(words $(OBJS))"
	@echo "============================================"

# ---- Compile C source files ----
# SRCS_C paths are like: cpu/uart/uart.c, ble_stack/user/le_rcsp_module.c
# These resolve relative to CWD (apps/) — e.g. apps/cpu/uart/uart.c
$(DIR_OUTPUT)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_ARGS) $(SYS_INCLUDES) $(INCLUDES) $(CC_ARG) -o $@ $<

# ---- Link object files + libraries into SDK ELF ----
# $^ = compiled objects (from our patched sources)
# $(LIBS) = pre-compiled .a libraries (../include_lib/*.a relative to apps/)
# $(SYS_LIBS) = libc.a + libcompiler-rt.a from toolchain
#
# LD_ARGS ends with "-o", so: $(LD) ... -o $@ $^ $(LIBS) $(SYS_LIBS)
$(OUTPUT_EXES): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LD_ARGS) $@ $^ $(LIBS) $(SYS_LIBS)
	@echo "Linked: $@ ($$(stat -c%s $@ 2>/dev/null || stat -f%z $@ 2>/dev/null) bytes)"

# ---- Post-build placeholder (handled by CI workflow steps) ----
post_build:
	@echo "Post-build handled by CI workflow"

.PHONY: all post_build pre_make
