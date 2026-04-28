This page outlines the build configurations and the CLI build commands for different platforms.

## VS Code Configurations

| Board Target     | Base configuration files (Kconfig fragments) | Extra Kconfig fragments | Base DeviceTree overlays         | Extra DeviceTree overlays | Snippets | Optimization level           | Extra CMake arguments |
|:-----------------|:---------------------------------------------|:------------------------|:---------------------------------|:--------------------------|:---------|:-----------------------------|:----------------------|
| rak4631/nrf52840 |                                              |                         | overlay/rak4631_nrf52840.overlay |                           |          | Optimize for debugging (-Og) |                       |


## CLI Commands

### rak4631/nrf52840

``` {.bash .copy}
west build --build-dir $(pwd)/build-rak4631 $(pwd) --pristine \
--board rak4631/nrf52840 -- -DCONF_FILE="prj.conf" \
-DDTC_OVERLAY_FILE=overlay/rak4631_nrf52840.overlay -DDEBUG_THREAD_INFO=Off \
-DCONFIG_DEBUG_OPTIMIZATIONS=y -Dfirmware_DEBUG_THREAD_INFO=Off \
-Dmcuboot_DEBUG_THREAD_INFO=Off
```

