TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += ADSimPeaks

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install ADSimPeaks.dbd into <top>/dbd
DBD += ADSimPeaks.dbd

# specify all source files to be compiled and added to the library
#ADSimPeaks_SRCS += xxx

ADSimPeaks_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

