@echo off
setlocal EnableExtensions DisableDelayedExpansion

pushd "%~dp0.." || exit /b 1

where cl.exe >nul 2>&1
if errorlevel 1 goto tools_error

where nmake.exe >nul 2>&1
if errorlevel 1 goto tools_error

if not exist third_party\sqlcipher\Makefile.msc goto source_error

set "NK_SQLCIPHER_PIN="
set "NK_SQLCIPHER_ACTUAL="

for /f "tokens=2" %%H in ('git ls-files --stage -- third_party/sqlcipher') do set "NK_SQLCIPHER_PIN=%%H"

for /f %%H in ('git -C third_party/sqlcipher rev-parse HEAD') do set "NK_SQLCIPHER_ACTUAL=%%H"

if not defined NK_SQLCIPHER_PIN goto source_error

if not "%NK_SQLCIPHER_PIN%"=="%NK_SQLCIPHER_ACTUAL%" goto source_error

git -C third_party/sqlcipher diff --quiet HEAD --
if errorlevel 1 goto source_error

set "NK_GEN_DIR=build\deps\sqlcipher-%NK_SQLCIPHER_PIN%"
set "NK_OUTPUT_DIR=third_party\prebuilt\sqlcipher"

if not exist "%NK_GEN_DIR%" mkdir "%NK_GEN_DIR%"
if errorlevel 1 goto failed

pushd "%NK_GEN_DIR%" || goto failed

nmake /NOLOGO /f ..\..\..\third_party\sqlcipher\Makefile.msc TOP=..\..\..\third_party\sqlcipher "EXT_FEATURE_FLAGS=-DSQLITE_HAS_CODEC -DSQLITE_TEMP_STORE=2" sqlite3.c sqlite3.h

set "NK_GENERATE_EXIT=%ERRORLEVEL%"
popd

if not "%NK_GENERATE_EXIT%"=="0" goto failed

findstr /C:"#define CIPHER_VERSION_NUMBER" "%NK_GEN_DIR%\sqlite3.c"
if errorlevel 1 goto failed

if not exist "%NK_OUTPUT_DIR%" mkdir "%NK_OUTPUT_DIR%"
if errorlevel 1 goto failed

copy /Y "%NK_GEN_DIR%\sqlite3.c" "%NK_OUTPUT_DIR%\sqlite3.c" >nul
if errorlevel 1 goto failed

copy /Y "%NK_GEN_DIR%\sqlite3.h" "%NK_OUTPUT_DIR%\sqlite3.h" >nul
if errorlevel 1 goto failed

copy /Y "third_party\sqlcipher\LICENSE.md" "%NK_OUTPUT_DIR%\LICENSE.md" >nul
if errorlevel 1 goto failed

> "%NK_OUTPUT_DIR%\source-commit.txt" echo %NK_SQLCIPHER_PIN%

echo.
echo SQLCipher source prepared successfully.
echo Output: %NK_OUTPUT_DIR%
echo Source commit: %NK_SQLCIPHER_PIN%

popd
exit /b 0

:tools_error
echo ERROR: Run this script from the x64 Visual Studio Developer Command Prompt.
goto failed

:source_error
echo ERROR: SQLCipher must match the repository's submodule reference.
echo Initialize the submodule and keep its tracked files unchanged.
goto failed

:failed
echo ERROR: SQLCipher source generation failed. Check the messages above.
popd
exit /b 1
