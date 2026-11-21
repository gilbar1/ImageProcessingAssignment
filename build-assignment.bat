@echo off
setlocal
cd /d "%~dp0"
if not exist lib\windows\glfw3.dll tar -xf BasicOpenGL-dependencies.zip
if not exist lib\windows\glfw3.dll exit /b 1
if not exist bin mkdir bin
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if errorlevel 1 exit /b 1
>bin\glfw.def echo LIBRARY glfw3.dll
>>bin\glfw.def echo EXPORTS
for %%f in (glfwInit glfwTerminate glfwWindowHint glfwCreateWindow glfwDestroyWindow glfwMakeContextCurrent glfwGetProcAddress) do >>bin\glfw.def echo %%f
lib /nologo /def:bin\glfw.def /machine:x64 /out:bin\glfw3.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /Iinclude /Isrc src\main.cpp src\Shader.cpp src\Debugger.cpp src\glad.c bin\glfw3.lib /Fo:bin\ /Fe:bin\assignment.exe
if errorlevel 1 exit /b 1
copy /y lib\windows\glfw3.dll bin\glfw3.dll >nul
bin\assignment.exe
