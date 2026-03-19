FORMAT_FILES=firmware/src/*.c firmware/include/*.h

format:
	clang-format -i $(FORMAT_FILES)
