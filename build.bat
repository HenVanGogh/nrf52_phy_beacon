@echo off
rem Build script for nRF52840 SHT sensor project
rem Usage: build.bat [sht31|sht41]

setlocal

if "%1"=="" (
    echo Please specify sensor type: sht31 or sht41
    exit /b 1
)

if "%1"=="sht31" (
    echo Building for SHT31 sensor...
    west build -p -b nrf52840dongle_nrf52840 --build-dir build_sht31_dongle -- -DCONF_FILE=boards/sht31_prj_dongle.conf -DDTC_OVERLAY_FILE=boards/sht31_nrf52840dongle_nrf52840.overlay
) else if "%1"=="sht41" (
    echo Building for SHT41 sensor...
    west build -p -b nrf52840dongle_nrf52840 --build-dir build_sht41_dongle -- -DCONF_FILE=boards/sht41_prj_dongle.conf -DDTC_OVERLAY_FILE=boards/sht41_nrf52840dongle_nrf52840.overlay
) else (
    echo Unknown sensor type: %1
    echo Please use sht31 or sht41
    exit /b 1
)

if %ERRORLEVEL% neq 0 (
    echo Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Build completed successfully
exit /b 0
