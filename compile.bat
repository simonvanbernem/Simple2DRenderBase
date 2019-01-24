@echo off
cd %~dp0
[ -d build ] || mkdir build
pushd build


IF "%1"=="release" (
	goto releaseL
) ELSE (
	goto debugL
)

:debugL
echo compiling in debug mode
set compiler_flags=-nologo -DDEBUG -Z7 -W4 -WX -wd4996 -wd4100 -wd4189 -utf-8
goto compileL

:releaseL
echo compiling in release mode
set compiler_flags=-nologo -DNBUI_ENABLE_IACA -Z7 -W4 -WX -wd4996 -wd4100 -wd4189 -utf-8 -Ox
goto compileL

:compileL
cl %compiler_flags% ../test.cpp user32.lib d3d11.lib d3dcompiler.lib

popd