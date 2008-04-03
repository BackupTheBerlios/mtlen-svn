#
# Gererated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc.exe
CCC=g++.exe
CXX=g++.exe
FC=

# Include project Makefile
include tlen8-Makefile.mk

# Object Directory
OBJECTDIR=build/ANSI/MinGW-Windows

# Object Files
OBJECTFILES=

# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS} 
	cd ExistingProjectRoot && ${MAKE} -f ExistingMakefile

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf:
	cd ExistingProjectRoot && ${MAKE} -f ExistingMakefile clean

# Subprojects
.clean-subprojects:
