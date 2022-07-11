
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
using std::cerr;
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
    m_maxPeaks(maxPeaks),
    m_initialized(false)
{

  string functionName(s_className + "::" + __func__);

  m_startEvent = epicsEventMustCreate(epicsEventEmpty);
  if (!m_startEvent) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "%s epicsEventCreate failure for start event.\n", functionName.c_str());
    return;
  }

  m_stopEvent = epicsEventMustCreate(epicsEventEmpty);
  if (!m_stopEvent) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "%s epicsEventCreate failure for stop event.\n", functionName.c_str());
    return;
  }

  //Add the params to the paramLib 
  createParam(ADSPIntegrateParamString, asynParamInt32, &ADSPIntegrateParam);
  
  //Initialize non static, non const, data members
  m_acquiring = 0;

  bool paramStatus = true;
  //Initialise any paramLib parameters that need passing up to device support
  paramStatus = ((setIntegerParam(ADAcquire, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADStatus, ADStatusIdle) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADAcquirePeriod, 1.0) == asynSuccess) && paramStatus);
  callParamCallbacks();
  if (!paramStatus) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "%s unable to set driver parameters in constructor.\n", functionName.c_str());
    return;
  }

  int status = asynSuccess;
  //Create the thread that produces the simulation data
  status = (epicsThreadCreate("ADSimPeaksTask",
			      epicsThreadPriorityHigh,
			      epicsThreadGetStackSize(epicsThreadStackMedium),
			      (EPICSTHREADFUNC)ADSimPeaksTaskC,
			      this) == NULL);
  if (status) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, \
	      "%s epicsThreadCreate failure for ADnEDEventTask.\n", functionName.c_str());
    return;
  }
  
  cout << functionName << " maxSizeX: " << m_maxSizeX << endl;
  cout << functionName << " maxSizeY: " << m_maxSizeY << endl;
  cout << functionName << " maxPeaks: " << m_maxPeaks << endl; 

  m_initialized = true;
  
}


ADSimPeaks::~ADSimPeaks()
{
  cout << __func__ << endl;
}

asynStatus ADSimPeaks::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  asynStatus status = asynSuccess;
  int imageMode = 0;
  int function = pasynUser->reason;
  
  string functionName(s_className + "::" + __func__);
  
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s entry...\n", functionName.c_str());

  getIntegerParam(ADImageMode, &imageMode);
  
  if (function == ADAcquire) {
    if ((value == 1) && (!m_acquiring)) {
      epicsEventSignal(this->m_startEvent);
      setIntegerParam(ADStatus, ADStatusAcquire);
    }
    if ((value == 0) && (m_acquiring)) {
      epicsEventSignal(this->m_stopEvent);
      if (imageMode == ADImageContinuous) {
          setIntegerParam(ADStatus, ADStatusIdle);
      } else {
	setIntegerParam(ADStatus, ADStatusAborted);
      }
    }
  }

  if (status != asynSuccess) {
    callParamCallbacks();
    return asynError;
  }

  status = (asynStatus) setIntegerParam(function, value);
  if (status != asynSuccess) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s error setting parameter. asynUser->reason: %d, value: %d\n", 
              functionName.c_str(), function, value);
    return(status);
  }
 
  callParamCallbacks();

  return status;

}

asynStatus ADSimPeaks::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
  
  asynStatus status = asynSuccess;
  //int function = pasynUser->reason;

  string functionName(s_className + "::" + __func__);
  
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s entry...\n", functionName.c_str());


  return status;

}

void ADSimPeaks::report(FILE *fp, int details)
{

  string functionName(s_className + "::" + __func__);
  cout << functionName << " portName:" << this->portName << endl;
  
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

bool ADSimPeaks::getInitialized(void)
{
  return m_initialized;
}

void ADSimPeaks::ADSimPeaksTask(void)
{
  int status = asynSuccess;
  epicsFloat64 updatePeriod = 0.0;
  epicsEventWaitStatus eventStatus;

  string functionName(s_className + "::" + __func__);

  m_acquiring = false;
  
  this->lock();
  while(true) {

    //Wait for a startEvent
    if (!m_acquiring) {
      
      this->unlock();
      eventStatus = epicsEventWait(m_startEvent);
      this->lock();
      if (eventStatus == epicsEventWaitOK) {
	asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
		  "%s starting simulation.\n", functionName.c_str());
	m_acquiring = true;
	setStringParam(ADStatusMessage, "Simulation Running");
	setIntegerParam(ADNumImagesCounter, 0);
      } else {
	asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
		  "%s eventStatus %d\n", functionName.c_str(), eventStatus);
      }
      
    }
    callParamCallbacks();

    if (m_acquiring) {
      //Get the acquire period, which we use to define the update rate
      getDoubleParam(ADAcquirePeriod, &updatePeriod);
      
      //Calculate Data
      
      //Wait for a stop event
      this->unlock();
      //epicsThreadSleep(updatePeriod);
      eventStatus = epicsEventWaitWithTimeout(m_stopEvent, updatePeriod);
      this->lock();
      if (eventStatus == epicsEventWaitOK) {
	asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
		  "%s stopping simulation.\n", functionName.c_str());
	m_acquiring = false;
	setIntegerParam(ADStatus, ADStatusIdle);
	setStringParam(ADStatusMessage, "Simulation Idle");
      }
      callParamCallbacks();
      
      cout << functionName << endl;
    }
    
  }
  
}



static void ADSimPeaksTaskC(void *drvPvt)
{
    ADSimPeaks *pPvt = (ADSimPeaks *)drvPvt;

    pPvt->ADSimPeaksTask();
}


extern "C" {

  asynStatus ADSimPeaksConfig(const char *portName, int maxSizeX, int maxSizeY, int maxPeaks,
			      int dataType, int maxBuffers, size_t maxMemory,
			      int priority, int stackSize)
  {
    asynStatus status = asynSuccess;
    
    // Instantiate class
    try {
      ADSimPeaks *adsp = new ADSimPeaks(portName, maxSizeX, maxSizeY, maxPeaks,
				       static_cast<NDDataType_t>(dataType), maxBuffers, maxMemory,
				       priority, stackSize);
      if (adsp->getInitialized()) {
	cerr << "Created ADSimPeaks OK." << endl;	
      } else {
	cerr << "Problem creating ADSimPeaks" << endl;
	status = asynError;
      }
    } catch (...) {
      cerr << __func__ << " exception caught when trying to construct ADSimPeaks." << endl;
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

