format:
	@cd firmware; make format
	@cd serial-driver/core; make format

docs:
	@doxygen serial-driver-core.doxygen
	@doxygen firmware.doxygen

.PHONY: docs
