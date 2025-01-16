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

set cl_link=-link -incremental:no -subsystem:console %dir_sdl%\lib\x64\SDL2main.lib %dir_sdl%\lib\x64\SDL2.lib shell32.lib
set clang_link=

set cl_debug=%cl_common% -W4 -WX -Z7 -DBUILD_DEBUG=1
REM -fsanitize=address
set clang_debug=%clang_common% -pedantic ^
    -Wall -Werror -Wextra -Wshadow -Wconversion -Wdouble-promotion ^
    -Wno-unused-function -Wno-sign-conversion -Wno-deprecated-declarations -fno-strict-aliasing ^
    -g3 -fsanitize=address,undefined -fsanitize-trap -DBUILD_DEBUG=1

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

if not exist SDL2.dll copy %dir_sdl%\lib\x64\SDL2.dll .
if exist %name%.pdb del %name%.pdb
%compile_debug% ..\src\main.c %compile_link% %compile_out%%name%.exe || exit /b 1

popd

REM @echo off
REM
REM setlocal
REM set name=bici
REM
REM for %%a in (%*) do set "%%a=1"
REM if not "%msvc%"=="1" if not "%clang%"=="1" set msvc=1
REM if not "%release%"=="1"                    set debug=1
REM
REM set dir_deps=..\deps
REM set dir_raylib=%dir_deps%\raylib-5.5_win64_msvc16
REM set include_paths=-I%dir_deps% -I%dir_raylib%\include
REM
REM set cl_common=cl -nologo -FC -diagnostics:column -std:c11 -MT %include_paths%
REM set clang_common=clang -pedantic -Wno-microsoft -std=c11 -MT %include_paths%
REM set cl_link=-link -incremental:no -nodefaultlib:libcmt %dir_raylib%\lib\raylib.lib gdi32.lib winmm.lib user32.lib shell32.lib
REM set clang_link=-Wl,-nodefaultlib:libcmt -l%dir_raylib%/lib/raylib -lgdi32 -lwinmm -luser32 -lshell32
REM set cl_debug=%cl_common% -W4 -WX -Z7 -DBUILD_DEBUG=1
REM set clang_debug=%clang_common% ^
REM     -Wall -Werror -Wextra -Wshadow -Wconversion -Wdouble-promotion ^
REM     -Wno-unused-function -Wno-sign-conversion -Wno-deprecated-declarations -fno-strict-aliasing ^
REM     -g3 -fsanitize=undefined -fsanitize-trap -DBUILD_DEBUG=1
REM set cl_out=-out:
REM set clang_out=-o
REM
REM if "%msvc%"=="1"  set compile_debug=%cl_debug%
REM if "%msvc%"=="1"  set compile_link=%cl_link%
REM if "%msvc%"=="1"  set compile_out=%cl_out%
REM if "%clang%"=="1" set compile_debug=%clang_debug%
REM if "%clang%"=="1" set compile_link=%clang_link%
REM if "%clang%"=="1" set compile_out=%clang_out%
REM
REM if not exist build mkdir build
REM pushd build
REM
REM if exist %name%.pdb del %name%.pdb
REM %compile_debug% ..\src\main.c %compile_link% %compile_out%%name%.exe
REM
REM popd
REM
