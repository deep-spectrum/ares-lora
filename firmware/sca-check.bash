source ~/.bashrc
rm -rf build-static-code-analysis
source ~/ncs/v3.2.2/zephyr/zephyr-env.sh
nrfutil sdk-manager toolchain launch --ncs-version v3.2.2 -- west build -b rak4631/nrf52840 --build-dir build-static-code-analysis $(pwd) -- -DDTC_OVERLAY_FILE=overlay/rak4631_nrf52840.overlay -DZEPHYR_SCA_VARIANT=codechecker -DCODECHECKER_EXPORT=html -DCODECHECKER_ANALYZE_OPTS="--analyzer-config;clang-tidy:take-config-from-directory=true;--ignore;$(pwd)/codechecker.skip;--drop-reports-from-skipped-files"
open build-static-code-analysis/firmware/sca/codechecker/codechecker.html/index.html
