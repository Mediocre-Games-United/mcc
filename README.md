# MCC (mcc compiler collection)

Simple all-in-one command line build system for C++ projects

Currently only for Linux. Windows may work (probably won't) but is very untested.

## INSTALLATION & USAGE

Follow the instructions at https://github.com/Mediocre-Games-United/mcc-build

To use, simply type mcc in any terminal for interactive mode.

Build and export files are saved in ./.mcc/ relative to project.mcc. If using git, add .mcc to .gitignore.

## MODES

### INTERACTIVE

In interactive mode, the program will guide the user in configuring, building and packaging. No need to learn build file syntax or troubleshoot it.

### STANDARD

Standard mode is not implemented yet, but in standard mode mcc will execute a command and quit, like a standard cli tool. For those who don't like the interactive mode or for automated systems that cannot interact.

## FEATURES

- Source to object files
- Recompiled on change only
- Recompiled on changes to dependencies
- Compilation modes
- Multithreading
- Monolithic executables
- Shared libraries
- Subprojects
- Packager
- Resource and lang packaging
- Runtime wrapper
- Config files for LSPs
- Cross Compiling to windows
- Copying dependent shared libraries
- Automatically installing dependencies from package managers
- Automatically installing dependencies from other sources

## FUTURE FEATURES

- Exporting full packages
- Distributing exported versions to providers such as itch.io
- Support for C with gcc and not g++
- Support for custom toolchains such as arm


## PROJECT STRUCTURE

The main branch will have the latest functional full version. The dev branch will have the latest development version.

Please make pull requests to dev and not main.
