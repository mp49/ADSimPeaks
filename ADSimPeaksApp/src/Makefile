TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

USR_CXXFLAGS += -std=c++11

LIBRARY_IOC += ADSimPeaks

DBD += ADSimPeaks.dbd

ADSimPeaks_SRCS += ADSimPeaks.cpp
ADSimPeaks_SRCS += ADSimPeaksData.cpp
ADSimPeaks_SRCS += ADSimPeaksPeak.cpp

ADSimPeaks_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

