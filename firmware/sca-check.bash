source ~/.bashrc
rm -rf build
source ~/ncs/v3.2.2/zephyr/zephyr-env.sh
nrfutil sdk-manager toolchain launch --ncs-version v3.2.2 -- west build -b rak4631/nrf52840 $(pwd) -- -DZEPHYR_SCA_VARIANT=codechecker -DCODECHECKER_EXPORT=html -DCODECHECKER_ANALYZE_OPTS="--analyzer-config;clang-tidy:take-config-from-directory=true;--ignore;$(pwd)/codechecker.skip;--drop-reports-from-skipped-files"
open build/firmware/sca/codechecker/codechecker.html/index.html
