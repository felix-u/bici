@echo off

setlocal
set name=bici

for %%a in (%*) do set "%%a=1"
if not "%msvc%"=="1" if not "%clang%"=="1" set msvc=1
if not "%release%"=="1"                    set debug=1

set cl_common=cl -nologo -FC -diagnostics:column -std:c11 -MT %include_paths%
set clang_common=clang -pedantic -Wno-microsoft -std=c11 -MT %include_paths%
set cl_link=-link -incremental:no user32.lib gdi32.lib
set clang_link=-luser32 -lgdi32
set cl_debug=%cl_common% -W4 -WX -Z7 -DBUILD_DEBUG=1
set clang_debug=%clang_common% ^
    -Wall -Werror -Wextra -Wshadow -Wconversion -Wdouble-promotion ^
    -Wno-unused-function -Wno-deprecated-declarations -Wno-missing-field-initializers -fno-strict-aliasing ^
    -g3 -fsanitize=undefined -fsanitize-trap -DBUILD_DEBUG=1
set cl_out=-out:
set clang_out=-o

if "%msvc%"=="1"  set compile_debug=%cl_debug%
if "%msvc%"=="1"  set compile_link=%cl_link%
if "%msvc%"=="1"  set compile_out=%cl_out%
if "%clang%"=="1" set compile_debug=%clang_debug%
if "%clang%"=="1" set compile_link=%clang_link%
if "%clang%"=="1" set compile_out=%clang_out%

if not exist build mkdir build
pushd build

if exist %name%.pdb del %name%.pdb
%compile_debug% ..\src\main.c %compile_link% %compile_out%%name%.exe

popd

