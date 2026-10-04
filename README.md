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

To use interactive mode, type `mcc` in a terminal. To create a temporary project, type `mcc -t`. To activate/open a file, type `mcc <filepath>`.

### STANDARD

Standard mode will execute a command and quit, ideal for automated systems. To use standard mode, simply type mcc \{command\}, such as `mcc -t run`, which will create a temporary project (-t) at current working directory and compile them all into a single executable and then runs it (run). To select a specific config, use `mcc --config <filepath> <command>` or `mcc --config-name <config name such as mcc> <command>`. To export project called "mcc", command `mcc --config-name mcc export` would be used.

Special commands that will instantly return are:
- `mcc -h` or `mcc --help` which will print the available --args and quit.
- `mcc --version` which will print the program version and quit.

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
