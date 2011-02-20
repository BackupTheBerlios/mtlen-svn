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
OBJECTDIR=build/Unicode/MinGW-Windows

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
	cd C\:/MirandaDev/miranda_svn2/protocols/tlen8 && ${MAKE} -f Makefile

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf:
	cd C\:/MirandaDev/miranda_svn2/protocols/tlen8 && ${MAKE} -f Makefile clean

# Subprojects
.clean-subprojects:
