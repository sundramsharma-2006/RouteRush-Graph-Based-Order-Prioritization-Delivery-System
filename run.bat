@echo off
REM Build and run RouteRush from this folder (no git or make needed).
REM Usage: double-click, or in a terminal:  run.bat            (demo)
REM                                          run.bat test       (unit tests)
REM                                          run.bat menu       (interactive menu)
REM                                          run.bat bench      (benchmarks)
REM                                          run.bat demo --fifo --pick=naive
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    echo g++ was not found. Install it with:
    echo     winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
    echo then close and reopen this window and run again.
    pause
    exit /b 1
)

set MODE=%1
if "%MODE%"=="" set MODE=demo
set FLAGS=-std=c++17 -O2 -Wall -Wextra -Iinclude -Itests

if /i "%MODE%"=="test" (
    setlocal enabledelayedexpansion
    set FILES=
    for %%f in (tests\*.cpp) do set FILES=!FILES! %%f
    g++ %FLAGS% !FILES! -o run_tests.exe || (pause & exit /b 1)
    run_tests.exe
    pause
    exit /b 0
)

if /i "%MODE%"=="bench" (
    g++ %FLAGS% bench\benchmark.cpp -o benchmark.exe || (pause & exit /b 1)
    benchmark.exe
    pause
    exit /b 0
)

if /i "%MODE%"=="menu" (
    g++ %FLAGS% src\main.cpp -o routerush.exe || (pause & exit /b 1)
    routerush.exe data --menu
    exit /b 0
)

REM demo (default); extra arguments are passed to the program
g++ %FLAGS% src\main.cpp -o routerush.exe || (pause & exit /b 1)
shift
routerush.exe data %1 %2 %3 %4
pause
