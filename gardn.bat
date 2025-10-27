@echo off
setlocal ENABLEDELAYEDEXPANSION

rem ====== paths & env ======
set "REPO=%~dp0"
if exist "C:\Program Files\CMake\bin\cmake.exe" (
  set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
) else (
  set "CMAKE=cmake"
)

rem EMSDK (assumes C:\emsdk). Change if needed.
call C:\Users\nicho\dev\emsdk\emsdk_env.bat >nul

rem ====== args ======
set "CMD=%~1"
if "%CMD%"=="" set "CMD=all"
set "PORT=%~2"
if "%PORT%"=="" set "PORT=9009"

if /I "%CMD%"=="client"  goto :client
if /I "%CMD%"=="server"  goto :server
if /I "%CMD%"=="run"     goto :run
if /I "%CMD%"=="kill"    goto :kill
if /I "%CMD%"=="clean"   goto :clean
if /I "%CMD%"=="all"     goto :all

echo Usage: gardn [client^|server^|run [port]^|kill [port]^|clean^|all [port]]
exit /b 1

:client
pushd "%REPO%Client"
if not exist build\CMakeCache.txt (
  "%CMAKE%" -S . -B build -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE=%EMSDK%\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER_LAUNCHER="cmd.exe;/c" ^
    -DCMAKE_CXX_COMPILER_LAUNCHER="cmd.exe;/c"
)
"%CMAKE%" --build build --target gardn-client --parallel || (popd & exit /b 1)
popd

rem ensure index.html exists (don’t overwrite if user customized)
if not exist "%REPO%index.html" (
  > "%REPO%index.html" (
    echo ^<!doctype html^>
    echo ^<meta charset="utf-8"^>^<title^>gardn^</title^>
    echo ^<style^>html,body{margin:0;height:100%%;background:#111;color:#ddd}canvas{display:block;width:100vw;height:100vh}^</style^>
    echo ^<canvas id="canvas" tabindex="1"^>^</canvas^>
    echo ^<script src="gardn-client.js"^>^</script^>
  )
)

rem hardlink (preferred) or copy client outputs to repo root
del /q "%REPO%gardn-client.js" "%REPO%gardn-client.wasm" 2>nul
mklink /H "%REPO%gardn-client.js"   "%REPO%Client\build\gardn-client.js"   >nul 2>nul || copy /Y "%REPO%Client\build\gardn-client.js"   "%REPO%" >nul
mklink /H "%REPO%gardn-client.wasm" "%REPO%Client\build\gardn-client.wasm" >nul 2>nul || copy /Y "%REPO%Client\build\gardn-client.wasm" "%REPO%" >nul
exit /b 0

:server
pushd "%REPO%Server"
rem Kill possible stray processes that can lock files
for %%P in (ninja.exe cmake.exe node.exe) do taskkill /IM %%P /F >nul 2>nul
if not exist build mkdir build
rem Try to clear possible locked ninja artifacts (ignore errors)
del /f /q build\.ninja_log build\.ninja_deps 2>nul


if not exist build\CMakeCache.txt (
  "%CMAKE%" -S . -B build -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE=%EMSDK%\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DWASM_SERVER=1 ^
    -DCMAKE_C_COMPILER_LAUNCHER="cmd.exe;/c" ^
    -DCMAKE_CXX_COMPILER_LAUNCHER="cmd.exe;/c" || (
      rem If configure fails (e.g., due to ninja log lock), wipe build dir and try once more
      rd /s /q build 2>nul & mkdir build & ^
      "%CMAKE%" -S . -B build -G Ninja ^
        -DCMAKE_TOOLCHAIN_FILE=%EMSDK%\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DWASM_SERVER=1 ^
        -DCMAKE_C_COMPILER_LAUNCHER="cmd.exe;/c" ^
        -DCMAKE_CXX_COMPILER_LAUNCHER="cmd.exe;/c"
    )
)
"%CMAKE%" --build build --target gardn-server --parallel || (
  rem If build fails, wipe build dir and rebuild once
  rd /s /q build 2>nul & ^
  "%CMAKE%" -S . -B build -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE=%EMSDK%\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DWASM_SERVER=1 ^
    -DCMAKE_C_COMPILER_LAUNCHER="cmd.exe;/c" ^
    -DCMAKE_CXX_COMPILER_LAUNCHER="cmd.exe;/c" && ^
  "%CMAKE%" --build build --target gardn-server --parallel || (popd & exit /b 1)
)

rem Ensure Node deps for the server runtime (ws, sqlite3)
if not exist node_modules (
  npm ci --silent
) else (
  npm i --silent
)
popd
exit /b 0



:run
set "GARDN_PORT=%PORT%"
rem If script-default port (9009) is being used, force runtime to 9001 for localhost
if "%GARDN_PORT%"=="9009" set "GARDN_PORT=9001"
rem Update PORT to the effective port before attempting to kill
set "PORT=%GARDN_PORT%"

rem ===== Hardened DB environment (safe defaults) =====
rem Use a single canonical DB path if not already provided by the user/env
if not defined SPETALS_DB_PATH set "SPETALS_DB_PATH=C:\spetals\data.db"
if not defined DB_BACKUP_DIR  set "DB_BACKUP_DIR=C:\spetals\db_backups"
if not defined DB_BACKUP_RETENTION set "DB_BACKUP_RETENTION=30"
if not defined ALLOW_ACCOUNT_CREATE set "ALLOW_ACCOUNT_CREATE=1"

rem Ensure directories exist
if not exist "C:\spetals" mkdir "C:\spetals" >nul 2>nul
if not exist "%DB_BACKUP_DIR%" mkdir "%DB_BACKUP_DIR%" >nul 2>nul

rem If DB file does not exist, allow initialization for this run only
set "_ALLOW_INIT_FOR_RUN="
if not exist "%SPETALS_DB_PATH%" (
  set "ALLOW_INIT_DB=1"
  set "_ALLOW_INIT_FOR_RUN=1"
)

call :kill >nul
pushd "%REPO%"

rem Pass env into node (server reads env vars)
set "PORT=%GARDN_PORT%"
node Server\build\gardn-server.js

rem Clear ALLOW_INIT_DB if we only set it for this run
if defined _ALLOW_INIT_FOR_RUN set "ALLOW_INIT_DB="
set "_ALLOW_INIT_FOR_RUN="

popd
exit /b 0



:all
call "%~f0" client            || exit /b 1
call "%~f0" server            || exit /b 1
call "%~f0" run "%PORT%"
exit /b 0

:kill
for /f "tokens=5" %%p in ('netstat -ano ^| findstr :%PORT%') do taskkill /PID %%p /F >nul 2>nul
exit /b 0

:clean
rd /s /q "%REPO%Client\build" 2>nul
rd /s /q "%REPO%Server\build" 2>nul
exit /b 0
