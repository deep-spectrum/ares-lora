format:
	@cd firmware; make format
	@cd serial-driver/core; make format

style:
	@cd serial-driver/core; make style

scc:
	@cd serial-driver/core; make scc

docs:
	@doxygen serial-driver-core.doxygen
	@doxygen firmware.doxygen

.PHONY: docs
