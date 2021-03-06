##
#
# This is the TriDAS/emu/emuDCS/FEDApps Package Makefile
#
##

include $(XDAQ_ROOT)/config/mfAutoconf.rules
include $(XDAQ_ROOT)/config/mfDefs.$(XDAQ_OS)
include $(BUILD_HOME)/emu/rpm_version

#CXX=g++4
#LD=g++4

#
# Packages to be built
#
Project=emu
Package=emuDCS/FEDApps
PackageName=FEDApps
Description="Applications to control and monitor the Front End Device (FED) crates for the CMS Emu (CSC) subdetector"
Summary="Emu (CSC) FED XDAQ Applications"
Authors="Phillip Killewald, Stan Durkin, Jason Gilmore, Jianhui Gu"
Link="http://physics.ohio-state.edu/~cms"

Sources=\
	AppsVersion.cc \
	Facts.cc \
	Application.cc \
	Manager.cc \
	Monitor.cc \
	Supervised.cc \
	Configurable.cc \
	IRQThreadManager.cc \
	Communicator.cc \
	MicroMonitor.cc \
	Commander.cc \
	FEDConfigurationEditor.cc \
	ConfigurationEditor.cc \
	EmuFCrateHyperDAQ.cc

IncludeDirs = \
	$(BUILD_HOME)/emu/base/include \
	$(BUILD_HOME)/emu/emuDCS/FEDCore/include \
	$(BUILD_HOME)/emu/emuDCS/FEDUtils/include \
	$(BUILD_HOME)/emu/emuDCS/FEDApps/include \
	$(BUILD_HOME)/emu/emuDCS/OnlineDB/include \
	$(XDAQ_ROOT)/include \
	$(XDAQ_ROOT)/include/$(XDAQ_OS)

LibraryDirs = \
	$(XDAQ_ROOT)/lib

UserCFlags =
UserCCFlags = -pipe -DLINUX -g -Wall -pedantic-errors -Wno-long-long -Wno-variadic-macros -isystem $(XDAQ_ROOT)/include -O1
UserStaticLinkFlags =
UserDynamicLinkFlags = -lboost_filesystem
UserExecutableLinkFlags = -D_REENTRAN -pthread -pedantic-errors

# These libraries can be platform specific and
# potentially need conditional processing
#
Libraries =
ExternalObjects =

#
# Compile the source files and create a shared library
#
DynamicLibrary = EmuFEDApps
StaticLibrary =

Executables =
TestExecutables =

include $(XDAQ_ROOT)/config/Makefile.rules
include $(XDAQ_ROOT)/config/mfRPM.rules
