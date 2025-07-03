@echo off
setlocal

if not exist build mkdir build

odin build src -out:build\bici.exe -debug -o:none -sanitize:address -vet-cast -vet-unused -vet-shadowing
