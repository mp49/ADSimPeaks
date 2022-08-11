#!../../bin/linux-x86_64/example

#- You may have to change example to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/example.dbd"
example_registerRecordDeviceDriver pdbbase

###############################################
# Start the ADSimPeaks driver (2D version)

ADSimPeaksConfig(D4.SIM,1024,1024,10,3,0,0,0,0)

NDCodecConfigure(D4.CO1,100,0,D4.SIM,0,0,0,0,0)

NDPvaConfigure(D4.PV1,100,0,D4.CO1,0,"ST99:Det:Det4:PV1:Array",0,0,0)

###############################################

## Load record instances
dbLoadRecords("db/example.db")

#################################################
# autosave

epicsEnvSet("IOCNAME","example2d")
epicsEnvSet("SAVE_DIR","/home/controls/var/$(IOCNAME)")

save_restoreSet_Debug(0)

### status-PV prefix, so save_restore can find its status PV's.
save_restoreSet_status_prefix("ST99:Det:IOC_SADet4:")

set_requestfile_path("$(SAVE_DIR)")
set_savefile_path("$(SAVE_DIR)")

save_restoreSet_NumSeqFiles(3)
save_restoreSet_SeqPeriodInSeconds(600)
set_pass0_restoreFile("$(IOCNAME).sav")
set_pass0_restoreFile("$(IOCNAME)_pass0.sav")
set_pass1_restoreFile("$(IOCNAME).sav")

#################################################


cd "${TOP}/iocBoot/${IOC}"
iocInit

# Create request file and start periodic 'save'
epicsThreadSleep(5)
makeAutosaveFileFromDbInfo("$(SAVE_DIR)/$(IOCNAME).req", "autosaveFields")
makeAutosaveFileFromDbInfo("$(SAVE_DIR)/$(IOCNAME)_pass0.req", "autosaveFields_pass0")
create_monitor_set("$(IOCNAME).req", 5)
create_monitor_set("$(IOCNAME)_pass0.req", 30)



