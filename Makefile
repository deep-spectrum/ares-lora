FORMAT_FILES=firmware/src/*.c firmware/include/*.h firmware/src/serial/*.c firmware/include/serial/*.h

format:
	clang-format -i $(FORMAT_FILES)
