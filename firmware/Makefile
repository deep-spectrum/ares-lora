EXCLUDE_DIRS := extern cmake-build* build*
FORMAT_EXTENSIONS := h c

PRUNE_ARGS := $(foreach dir,$(EXCLUDE_DIRS),-path "./$(dir)" -prune -o)

FIRST_EXT := $(firstword $(FORMAT_EXTENSIONS))
REST_EXTS := $(wordlist 2, $(words $(FORMAT_EXTENSIONS)), $(FORMAT_EXTENSIONS))
FORMAT_FILES := -name "*.$(FIRST_EXT)" $(foreach ext,$(REST_EXTS),-o -name "*.$(ext)")

format:
	@find . $(PRUNE_ARGS) -type f \( $(FORMAT_FILES) \) -print | xargs clang-format -i