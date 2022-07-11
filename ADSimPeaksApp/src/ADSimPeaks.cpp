
//Standard 
#include <iostream>
#include <string>
#include <stdexcept>

//EPICS
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsExport.h>
#include <epicsString.h>
#include <iocsh.h>
#include <drvSup.h>
#include <registryFunction.h>

//ADSimPeaks
#include "ADSimPeaks.h"

using std::cout;
using std::endl;

static void ADSimPeaksTaskC(void *drvPvt);

ADSimPeaks::ADSimPeaks(const char *portName, int maxSizeX, int maxSizeY, int maxPeaks,
		       NDDataType_t dataType, int maxBuffers, size_t maxMemory,
		       int priority, int stackSize)
  : ADDriver(portName, 1, 0, maxBuffers, maxMemory, 0, 0, 0, 1, priority, stackSize),
    m_maxSizeX(maxSizeX),
    m_maxSizeY(maxSizeY),
    m_maxPeaks(maxPeaks)
{
  std::cout << "Creating ADSimPeaks..." << std::endl;

  std::cout << __func__ << " maxSizeX: " << m_maxSizeX << endl;
  std::cout << __func__ << " maxSizeY: " << m_maxSizeY << endl;
  std::cout << __func__ << " maxPeaks: " << m_maxPeaks << endl; 
}


ADSimPeaks::~ADSimPeaks()
{
  std::cout << __func__ << endl;
}


asynStatus ADSimPeaks::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  asynStatus status = asynSuccess;
  //int function = pasynUser->reason;
  
  const char *functionName = "ADSimPeaks::writeInt32";
  
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s entry...\n", functionName);

  cout << __func__ << " entry..." << endl;

  return status;

}



extern "C" {

  asynStatus  ADSimPeaksConfig(const char *portName, int maxSizeX, int maxSizeY, int maxPeaks,
			       int dataType, int maxBuffers, size_t maxMemory,
			       int priority, int stackSize)
  {
    asynStatus status = asynSuccess;
    
    // Instantiate class
    try {
      new ADSimPeaks(portName, maxSizeX, maxSizeY, maxPeaks,
		     static_cast<NDDataType_t>(dataType), maxBuffers, maxMemory,
		     priority, stackSize);
    } catch (...) {
      cout << __func__ << "Unknown exception caught when trying to construct ADSimPeaks." << endl;
      status = asynError;
    }
    
    return(status);
    
  }
  
  // Code for iocsh registration
  static const iocshArg ADSimPeaksConfigArg0 = {"Port Name", iocshArgString};
  static const iocshArg ADSimPeaksConfigArg1 = {"Max X Size", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg2 = {"Max Y Size", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg3 = {"Max Peaks", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg4 = {"Data Type", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg5 = {"maxBuffers", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg6 = {"maxMemory", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg7 = {"priority", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg8 = {"stackSize", iocshArgInt};
  static const iocshArg * const ADSimPeaksConfigArgs[] =  {&ADSimPeaksConfigArg0,
							   &ADSimPeaksConfigArg1,
							   &ADSimPeaksConfigArg2,
							   &ADSimPeaksConfigArg3,
							   &ADSimPeaksConfigArg4,
							   &ADSimPeaksConfigArg5,
							   &ADSimPeaksConfigArg6,
							   &ADSimPeaksConfigArg7,
							   &ADSimPeaksConfigArg8};
  static const iocshFuncDef configADSimPeaks = {"ADSimPeaksConfig", 9, ADSimPeaksConfigArgs};
  static void configADSimPeaksCallFunc(const iocshArgBuf *args)
  {
    ADSimPeaksConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
		     args[4].ival, args[5].ival, args[6].ival, args[7].ival, args[8].ival);
  }
  
  static void ADSimPeaksRegister(void)
  {
    
    iocshRegister(&configADSimPeaks, configADSimPeaksCallFunc);
  }
  
    epicsExportRegistrar(ADSimPeaksRegister);

} // Extern "C"

