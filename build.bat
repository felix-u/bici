@echo off

setlocal
set name=bici

for %%a in (%*) do set "%%a=1"
if not "%msvc%"=="1" if not "%clang%"=="1" set msvc=1
if not "%release%"=="1"                    set debug=1

set dir_deps=..\deps
set shdc=%dir_deps%\sokol\sokol-shdc.exe
set include_paths=-I%dir_deps%

set cl_common=cl -nologo -FC -diagnostics:column -std:c11 -Oi %include_paths%
set clang_common=clang -pedantic -Wno-microsoft -Wno-gnu-zero-variadic-macro-arguments -Wno-dollar-in-identifier-extension -std=c11 %include_paths%
set cl_link=-DLINK_CRT=1 -link -subsystem:console -incremental:no -opt:ref -fixed libucrtd.lib
set clang_link=-DLINK_CRT=1
set cl_debug=%cl_common% -MTd -W4 -WX -Z7 -DBUILD_DEBUG=1 -fsanitize=address
set clang_debug=%clang_common% ^
    -Wall -Werror -Wextra -Wshadow -Wconversion -Wdouble-promotion ^
    -Wno-unused-function -Wno-deprecated-declarations -Wno-missing-field-initializers -Wno-unused-local-typedef -Wno-initializer-overrides -fno-strict-aliasing ^
    -g3 -fsanitize=undefined -fsanitize-trap -DBUILD_DEBUG=1 -fsanitize=address
set cl_out=-out:
set clang_out=-o

REM TODO(felix): asan for debug! requires CRT
REM TODO(felix): also MSVC's -RTC options (requires CRT)

set cl_release=%cl_common% -O2 -MT
set clang_release=%clang_common% -MT -O3 -Wno-assume

if "%msvc%"=="1"  set compile_debug=%cl_debug%
if "%msvc%"=="1"  set compile_release=%cl_release%
if "%msvc%"=="1"  set compile_link=%cl_link%
if "%msvc%"=="1"  set compile_out=%cl_out%
if "%clang%"=="1" set compile_debug=%clang_debug%
if "%clang%"=="1" set compile_release=%clang_release%
if "%clang%"=="1" set compile_link=%clang_link%
if "%clang%"=="1" set compile_out=%clang_out%

if "%debug%"=="1" set compile=%compile_debug%
if "%release%"=="1" set compile=%compile_release%

if not exist build mkdir build
pushd build

if not exist sokol.obj (
    %compile_debug% -DSOKOL_IMPL -c ..\src\sokol.c
    if %errorlevel% neq 0 (
        popd
        exit /b 1
    )
)

if exist ..\src\shader.c del ..\src\shader.c
%shdc% -l hlsl5 -i ..\src\shader.glsl -o ..\src\shader.c -e msvc
if %errorlevel% neq 0 (
    popd
    exit /b 1
)

if exist %name%.pdb del %name%.pdb
%compile% ..\src\main.c sokol.obj %compile_link% %compile_out%%name%.exe
if %errorlevel% neq 0 (
    popd
    exit /b 1
)

popd
