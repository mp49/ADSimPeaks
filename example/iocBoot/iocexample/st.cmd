#!../../bin/linux-x86_64/example

#- You may have to change example to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/example.dbd"
example_registerRecordDeviceDriver pdbbase

###############################################
# Start the ADSimPeaks driver

ADSimPeaksConfig(D1.SIM,1024,0,10,3,0,0,0,0)

###############################################

## Load record instances
dbLoadRecords("db/example.db")

cd "${TOP}/iocBoot/${IOC}"
iocInit


