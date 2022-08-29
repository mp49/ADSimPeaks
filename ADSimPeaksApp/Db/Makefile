TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this
DB += ADSimPeaks.template
DB += ADSimPeaksNBackground.template
DB += ADSimPeaks1DBackground.template
DB += ADSimPeaks2DBackground.template
DB += ADSimPeaksNPeak.template
DB += ADSimPeaksPeakCommon.template
DB += ADSimPeaks1DPeak.template
DB += ADSimPeaks2DPeak.template

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

