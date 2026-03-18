FORMAT_FILES=firmware/src/*.c

format:
	clang-format -i $(FORMAT_FILES)
