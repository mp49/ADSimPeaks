
//Standard 
#include <iostream>
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
using std::string;

static void ADSimPeaksTaskC(void *drvPvt);

//Static Data
const string ADSimPeaks::s_className = "ADSimPeaks";

ADSimPeaks::ADSimPeaks(const char *portName, int maxSizeX, int maxSizeY, int maxPeaks,
		       NDDataType_t dataType, int maxBuffers, size_t maxMemory,
		       int priority, int stackSize)
  : ADDriver(portName, 1, 0, maxBuffers, maxMemory, 0, 0, 0, 1, priority, stackSize),
    m_maxSizeX(maxSizeX),
    m_maxSizeY(maxSizeY),
    m_maxPeaks(maxPeaks)
{

  string functionName(s_className + "::" + __func__);
  
  cout << functionName << " maxSizeX: " << m_maxSizeX << endl;
  cout << functionName << " maxSizeY: " << m_maxSizeY << endl;
  cout << functionName << " maxPeaks: " << m_maxPeaks << endl; 

  cout << "Created " << s_className << " OK." << endl;
}


ADSimPeaks::~ADSimPeaks()
{
  cout << __func__ << endl;
}


asynStatus ADSimPeaks::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  asynStatus status = asynSuccess;
  int function = pasynUser->reason;
  
  string functionName(s_className + "::" + __func__);
  
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s entry...\n", functionName.c_str());

  return status;

}

asynStatus ADSimPeaks::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
  
  asynStatus status = asynSuccess;
  //int function = pasynUser->reason;

  const char *functionName = "ADSimPeaks::writeFloat64";
  
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s entry...\n", functionName);

  cout << __func__ << " entry..." << endl;

  return status;

}

void ADSimPeaks::report(FILE *fp, int details)
{
  
  fprintf(fp, "ADSimPeaks %s\n", this->portName);
  if (details > 0) {
    cout << __func__ << endl;
    cout << "  m_maxSizeX: " << m_maxSizeX << endl;
    cout << "  m_maxSizeY: " << m_maxSizeY << endl;
    cout << "  m_maxPeaks: " << m_maxPeaks << endl;

    
    /*int nx, ny, dataType;
    getIntegerParam(ADSizeX, &nx);
    getIntegerParam(ADSizeY, &ny);
    getIntegerParam(NDDataType, &dataType);
    fprintf(fp, "  NX, NY:            %d  %d\n", nx, ny);
    fprintf(fp, "  Data type:         %d\n", dataType);*/
  }
  /* Invoke the base class method */
  ADDriver::report(fp, details);
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

