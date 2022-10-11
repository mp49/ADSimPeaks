TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = example
# example.dbd will be created and installed
DBD += example.dbd

# example.dbd will be made up from these files:
example_DBD += base.dbd
example_DBD += asyn.dbd
example_DBD += ADSupport.dbd
example_DBD += NDPluginSupport.dbd
example_DBD += NDPluginPva.dbd
example_DBD += ADSimPeaks.dbd
example_DBD += busySupport.dbd
example_DBD += asSupport.dbd

example_DBD += PVAServerRegister.dbd
example_DBD += qsrv.dbd

# Add all the support libraries needed by this IOC
example_LIBS += asyn
example_LIBS += ADBase
example_LIBS += NDPlugin
example_LIBS += ADSimPeaks
example_LIBS += busy
example_LIBS += autosave

example_LIBS += qsrv
example_LIBS += $(EPICS_BASE_PVA_CORE_LIBS)

# example_registerRecordDeviceDriver.cpp derives from example.dbd
example_SRCS += example_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
example_SRCS_DEFAULT += exampleMain.cpp
example_SRCS_vxWorks += -nil-

# Finally link to the EPICS Base libraries
example_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

