@echo off
zig build

REM setlocal
REM set name=bici
REM
REM for %%a in (%*) do set "%%a=1"
REM if not "%msvc%"=="1" if not "%clang%"=="1" set msvc=1
REM if not "%release%"=="1"                    set debug=1
REM
REM set dir_deps=..\deps
REM set include_paths=-I%dir_deps%
REM
REM set cl_common=cl -nologo -std:c11 %include_paths% 
REM set clang_common=clang -std=c11 %include_paths%
REM set cl_link=-link -incremental:no
REM set clang_link=
REM set cl_debug=%cl_common% -W4 -WX -Z7 -DBUILD_DEBUG=1 -fsanitize=address
REM set clang_debug=%clang_common% -pedantic ^
REM     -Wall -Werror -Wextra -Wshadow -Wconversion -Wdouble-promotion ^
REM     -Wno-unused-function -Wno-sign-conversion -Wno-deprecated-declarations -fno-strict-aliasing ^
REM     -g3 -fsanitize=address,undefined -fsanitize-trap -DBUILD_DEBUG=1
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
