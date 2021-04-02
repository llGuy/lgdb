@echo off
msbuild -noLogo -warnAsError -verbosity:minimal ../../build/lgdb.sln
