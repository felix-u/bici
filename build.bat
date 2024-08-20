@echo off

setlocal
set name=bici

for %%a in (%*) do set "%%a=1"
if not "%msvc%"=="1" if not "%clang%"=="1" set msvc=1
if not "%release%"=="1"                    set debug=1

set dir_deps=..\deps
set dir_sdl=%dir_deps%\SDL2-2.30.6-win32
set include_paths=-I%dir_sdl%\include

set cl_common=cl -nologo -FC -diagnostics:column -std:c11 %include_paths% 
set clang_common=clang -std=c11 %include_paths%
set cuik_common=C:\Users\Felix\Downloads\Cuik\bin\cuik -lang c11 %include_paths%

set cl_link=-link -incremental:no -subsystem:console %dir_sdl%\lib\x64\SDL2main.lib %dir_sdl%\lib\x64\SDL2.lib shell32.lib
set clang_link=
set cuik_link=-subsystem console -L%dir_sdl%\lib\x64 -lSDL2main -lSDL2 shell32

set cl_debug=%cl_common% -W4 -WX -Z7 -DBUILD_DEBUG=1 -fsanitize=address
set clang_debug=%clang_common% -pedantic ^
    -Wall -Werror -Wextra -Wshadow -Wconversion -Wdouble-promotion ^
    -Wno-unused-function -Wno-sign-conversion -Wno-deprecated-declarations -fno-strict-aliasing ^
    -g3 -fsanitize=address,undefined -fsanitize-trap -DBUILD_DEBUG=1
set cuik_debug=%cuik_common% -DBUILD_DEBUG=1 -g

set cl_out=-out:
set clang_out=-o
set cuik_out=-o

if "%msvc%"=="1"  set compile_debug=%cl_debug%
if "%msvc%"=="1"  set compile_link=%cl_link%
if "%msvc%"=="1"  set compile_out=%cl_out%

if "%clang%"=="1" set compile_debug=%clang_debug%
if "%clang%"=="1" set compile_link=%clang_link%
if "%clang%"=="1" set compile_out=%clang_out%

if "%cuik%"=="1" set compile_debug=%cuik_debug%
if "%cuik%"=="1" set compile_link=%cuik_link%
if "%cuik%"=="1" set compile_out=%cuik_out%

if not exist build mkdir build
pushd build

if not exist SDL2.dll copy %dir_sdl%\lib\x64\SDL2.dll .
if exist %name%.pdb del %name%.pdb
%compile_debug% ..\src\main.c %compile_link% %compile_out%%name%.exe || exit /b 1

popd
