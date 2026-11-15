@echo off
setlocal
set "APP_DIR=%~dp0"
set "GST_PLUGIN_PATH=%APP_DIR%lib\gstreamer-1.0"
set "GST_PLUGIN_PATH_1_0=%APP_DIR%lib\gstreamer-1.0"
set "GST_PLUGIN_SYSTEM_PATH_1_0=%APP_DIR%lib\gstreamer-1.0"
set "GST_PLUGIN_SCANNER_1_0=%APP_DIR%libexec\gstreamer-1.0\gst-plugin-scanner.exe"
if not exist "%LOCALAPPDATA%\RAEL_PLAY" mkdir "%LOCALAPPDATA%\RAEL_PLAY"
set "GST_REGISTRY_1_0=%LOCALAPPDATA%\RAEL_PLAY\registry.bin"
set "GIO_EXTRA_MODULES=%APP_DIR%lib\gio\modules"
set "SSL_CERT_FILE=%APP_DIR%etc\ssl\certs\ca-bundle.crt"
set "OPENSSL_CONF=%APP_DIR%etc\ssl\openssl.cnf"
set "FONTCONFIG_PATH=%APP_DIR%etc\fonts"
set "FONTCONFIG_FILE=%APP_DIR%etc\fonts\fonts.conf"
set "PATH=%APP_DIR%;%SystemRoot%\System32;%SystemRoot%"
pushd "%APP_DIR%"
start "" "%APP_DIR%raelplay.exe"
popd
endlocal