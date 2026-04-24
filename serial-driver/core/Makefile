EXCLUDE_DIRS := extern cmake-build* build* .venv*
FORMAT_EXTENSIONS := h hpp c cpp

PRUNE_ARGS := $(foreach dir,$(EXCLUDE_DIRS),-path "./$(dir)" -prune -o)

FIRST_EXT := $(firstword $(FORMAT_EXTENSIONS))
REST_EXTS := $(wordlist 2, $(words $(FORMAT_EXTENSIONS)), $(FORMAT_EXTENSIONS))
FORMAT_FILES := -name "*.$(FIRST_EXT)" $(foreach ext,$(REST_EXTS),-o -name "*.$(ext)")

INCLUDE_FLAGS := $(shell find . -type d -name include ! -path './extern/*' ! -path './.*/*' -printf '-I%p ')
PYTHON_INCLUDES := $(shell python3-config --includes | tr ' ' '\n' | sort -u | tr '\n' ' ')

format:
	@find . $(PRUNE_ARGS) -type f \( $(FORMAT_FILES) \) -print | xargs clang-format -i

style:
	clang-tidy -header-filter='^((?!extern/).)*$$' \
    		$$(find . -path "./extern" -prune -o -path "./.*" -prune -o $(FORMAT_FILES) -print) \
    		-- -std=c++17 $(PYTHON_INCLUDES) $(INCLUDE_FLAGS) \
    		-isystem extern/pybind11/include -isystem extern/ares-common/include -isystem extern/ares-common/include/common
