@echo off

setlocal

if not [%1] == [] if not [%2] == [] set cmake_args=-D%1=%2
if not [%3] == [] if not [%4] == [] set cmake_args=%cmake_args% -D%3=%4

if not exist debug mkdir debug
cd debug

if exist CMakeCache.txt (del CMakeCache.txt)

call "D:/Program Files (x86)/Microsoft Visual Studio/2017/Community/Common7/Tools/VsDevCmd.bat"

cmake -G "Visual Studio 15" -DCMAKE_BUILD_TYPE=Debug %cmake_args% ../src

msbuild fx_tools.sln /m

cd ..

@echo on