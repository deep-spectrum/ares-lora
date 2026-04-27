EXCLUDE_DIRS := extern cmake-build* build*
FORMAT_EXTENSIONS := h c

PRUNE_ARGS := $(foreach dir,$(EXCLUDE_DIRS),-path "./$(dir)" -prune -o)

FIRST_EXT := $(firstword $(FORMAT_EXTENSIONS))
REST_EXTS := $(wordlist 2, $(words $(FORMAT_EXTENSIONS)), $(FORMAT_EXTENSIONS))
FORMAT_FILES := -name "*.$(FIRST_EXT)" $(foreach ext,$(REST_EXTS),-o -name "*.$(ext)")

format:
	@find . $(PRUNE_ARGS) -type f \( $(FORMAT_FILES) \) -print | xargs clang-format -i

scc:
	@bash -c 'CODE_FILES=$$(find src include */src */include \( -name "*.c" -o -name "*.h" \) 2>/dev/null); \
	printf "SCC Report:\n"; \
	printf "Status\tSCC\tfilename(definition line number):function\n"; \
	pmccabe $$CODE_FILES 2>/dev/null | awk -v r="$$(tput setaf 1)" -v g="$$(tput setaf 2)" -v y="$$(tput setaf 3)" -v n="$$(tput sgr0)" " \
	{ \
		if (\$$1 <= 10) \
			printf \"%sOK\t%s\t%s %s%s\n\", g, \$$1, \$$6, \$$7, n; \
		else if (\$$1 <= 15) \
			printf \"%sWARNING\t%s\t%s %s%s\n\", y, \$$1, \$$6, \$$7, n; \
		else \
			printf \"%sERROR\t%s\t%s %s%s\n\", r, \$$1, \$$6, \$$7, n; \
	}"'