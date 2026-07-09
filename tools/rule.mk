# rule.mk - CC4060 Firmware Build Rules
# This file is included by apps/Makefile via -include $(MAKE_RULE)
# It provides the compile, link, and post-build rules for the SDK.

# ---- Object files ----
OBJS = $(addprefix $(DIR_OUTPUT)/obj/, $(patsubst %.c,%.o, $(SRCS_C)))

# ---- Default target ----
all: pre_make $(OUTPUT_EXES) post_build

# ---- Compile C source files ----
$(DIR_OUTPUT)/obj/%.o: apps/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_ARGS) $(SYS_INCLUDES) $(INCLUDES) $(CC_ARG) -o $@ $<

# ---- Link object files into SDK ELF ----
$(OUTPUT_EXES): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LD_ARGS) $(LINKER) $^ $(LIBS_PATH) $(patsubst ../%,$(CURDIR)/apps/%,$(LIBS)) $(SYS_LIBS)

# ---- Post-build: objcopy + firmware image + BFU packaging ----
post_build:
	@echo "========== POST BUILD =========="
	cd $(CURDIR) && bash build.sh post_build
	@echo "========== BUILD COMPLETE =========="

.PHONY: all post_build pre_make
