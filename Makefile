FORMAT_FILES=firmware/src/*.c firmware/include/*.h firmware/src/serial/*.c firmware/include/serial/*.h firmware/src/lora/*.c firmware/include/lora/*.h

format:
	clang-format -i $(FORMAT_FILES)
