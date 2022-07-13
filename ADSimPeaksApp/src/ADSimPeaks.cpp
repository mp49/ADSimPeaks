
//Standard 
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

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
const epicsFloat64 ADSimPeaks::s_zeroCheck = 1e-12;

ADSimPeaks::ADSimPeaks(const char *portName, int maxSize, int maxPeaks,
		       NDDataType_t dataType, int maxBuffers, size_t maxMemory,
		       int priority, int stackSize)
  : ADDriver(portName, 1, 0, maxBuffers, maxMemory, 0, 0, 0, 1, priority, stackSize),
    m_maxSize(maxSize),
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
  createParam(ADSPElapsedTimeParamString, asynParamFloat64, &ADSPElapsedTimeParam);
  createParam(ADSPPeakTypeParamString, asynParamInt32, &ADSPPeakTypeParam);
  createParam(ADSPPeakPosParamString, asynParamFloat64, &ADSPPeakPosParam);
  createParam(ADSPPeakFWHMParamString, asynParamFloat64, &ADSPPeakFWHMParam);
  createParam(ADSPPeakMaxParamString, asynParamFloat64, &ADSPPeakMaxParam);
  
  //Initialize non static, non const, data members
  m_acquiring = 0;
  m_uniqueId = 0;
  m_needNewArray = true;
  m_needReset = false;

  bool paramStatus = true;
  //Initialise any paramLib parameters that need passing up to device support
  paramStatus = ((setIntegerParam(ADAcquire, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADStatus, ADStatusIdle) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADAcquirePeriod, 1.0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADMaxSizeX, m_maxSize) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSizeX, m_maxSize) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPElapsedTimeParam, 0.0) == asynSuccess) && paramStatus);
  //Peak Params
  paramStatus = ((setIntegerParam(ADSPPeakTypeParam, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPPeakPosParam, 1.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPPeakFWHMParam, 1.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPPeakMaxParam, 1.0) == asynSuccess) && paramStatus);
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
  
  cout << functionName << " maxSize: " << m_maxSize << endl;
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
      m_needReset = true;
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
  } else if (function == ADSizeX) {
    value = std::max(1, std::min(static_cast<int32_t>(value), static_cast<int32_t>(m_maxSize)));
    int currentXSize = 0;
    getIntegerParam(ADSizeX, &currentXSize);
    if (value != currentXSize) {
      m_needNewArray = true;
    }
  } else if (function == NDDataType) {
    m_needNewArray = true;  
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
  int function = pasynUser->reason;

  string functionName(s_className + "::" + __func__);
  
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s entry...\n", functionName.c_str());

  if (status != asynSuccess) {
    callParamCallbacks();
    return asynError;
  }

  status = (asynStatus) setDoubleParam(function, value);
  if (status != asynSuccess) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s error setting parameter. asynUser->reason: %d, value: %f\n", 
              functionName.c_str(), function, value);
    return(status);
  }
 
  callParamCallbacks();

  return status;

}

void ADSimPeaks::report(FILE *fp, int details)
{

  string functionName(s_className + "::" + __func__);
  cout << functionName << " portName:" << this->portName << endl;
  
  if (details > 0) {
    cout << __func__ << endl;
    cout << "  m_maxSize: " << m_maxSize << endl;
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
  int size = 0;
  int ndims=0;
  size_t dims[1];
  int dataTypeInt = 0;
  NDDataType_t dataType;
  NDArrayInfo_t arrayInfo;
  epicsTimeStamp startTime;
  epicsTimeStamp nowTime;
  int arrayCounter = 0;
  int imagesCounter = 0;
  int arrayCallbacks = 0;
  epicsFloat64 updatePeriod = 0.0;
  double elapsedTime = 0.0;
  epicsEventWaitStatus eventStatus;

  string functionName(s_className + "::" + __func__);

  p_NDArray = NULL;
  m_acquiring = false;
  
  this->lock();
  while(true) {

    //Wait for a startEvent
    if (!m_acquiring) {
      
      this->unlock();
      eventStatus = epicsEventWait(m_startEvent);
      this->lock();
      imagesCounter = 0;
      if (eventStatus == epicsEventWaitOK) {
	asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
		  "%s starting simulation.\n", functionName.c_str());
	m_acquiring = true;
	setStringParam(ADStatusMessage, "Simulation Running");
	setIntegerParam(ADNumImagesCounter, 0);
	epicsTimeGetCurrent(&startTime);
      } else {
	asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s eventStatus %d\n", functionName.c_str(), eventStatus);
      }  
    }
    callParamCallbacks();

    if (m_acquiring) {
      getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
      
      ++arrayCounter;
      ++imagesCounter;
      
      getIntegerParam(NDDataType, &dataTypeInt);
      dataType = (NDDataType_t)dataTypeInt;
      getIntegerParam(ADSizeX, &size);
      ndims = 1;
      dims[0] = size;

      if (m_needNewArray) {
	if (p_NDArray != NULL) {
	  p_NDArray->release();
	  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s released NDArray\n", functionName.c_str());
	  //TODO - do I need to empty the free list, otherwise the pool keeps growing each time we change the array size?
	}
	if ((p_NDArray = this->pNDArrayPool->alloc(ndims, dims, dataType, 0, NULL)) == NULL) {
	  asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s failed to alloc NDArray\n", functionName.c_str());
	} else {
	  m_needNewArray = false;
	  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s allocated new NDArray\n", functionName.c_str());
	}
      }

      if (p_NDArray != NULL) {
	//Generate sim data here
	if (computeData(dataType) != asynSuccess) {
	  asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s failed to compute data.\n", functionName.c_str());
	}
	
	epicsTimeGetCurrent(&nowTime);
	elapsedTime = epicsTimeDiffInSeconds(&nowTime, &startTime);
        p_NDArray->uniqueId = arrayCounter;
	p_NDArray->timeStamp = nowTime.secPastEpoch + nowTime.nsec / 1.e9;
	updateTimeStamp(&p_NDArray->epicsTS);
	setDoubleParam(NDTimeStamp, p_NDArray->timeStamp);
	setDoubleParam(ADSPElapsedTimeParam, elapsedTime);
	
	p_NDArray->getInfo(&arrayInfo);
	setIntegerParam(NDArraySize, arrayInfo.totalBytes);
	setIntegerParam(NDArraySizeX, dims[0]);
	setIntegerParam(NDArrayCounter, arrayCounter);
	setIntegerParam(ADNumImagesCounter, imagesCounter);
	
	this->getAttributes(p_NDArray->pAttributeList);
	
	if (arrayCallbacks) {	  
	  doCallbacksGenericPointer(p_NDArray, NDArrayData, 0);
	}
	callParamCallbacks();
      }
      
      //Get the acquire period, which we use to define the update rate
      getDoubleParam(ADAcquirePeriod, &updatePeriod);
      
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
    }
    
  }
  
}


asynStatus ADSimPeaks::computeData(NDDataType_t dataType)
{
  asynStatus status = asynSuccess;
  //NDDataType_t dataType;
  NDArrayInfo_t arrayInfo;

  string functionName(s_className + "::" + __func__);

  if (dataType == NDInt8) {
    status = computeDataT<epicsInt8>();
  } else if (dataType == NDUInt8) {
    status = computeDataT<epicsUInt8>();
  } else if (dataType == NDInt16) {
    status = computeDataT<epicsInt16>();
  } else if (dataType == NDUInt16) {
    status = computeDataT<epicsUInt16>();
  } else if (dataType == NDInt32) {
    status = computeDataT<epicsInt32>();
  } else if (dataType == NDUInt32) {
    status = computeDataT<epicsUInt32>();
  } else if (dataType == NDInt64) {
    status = computeDataT<epicsInt64>();
  } else if (dataType == NDUInt64) {
    status = computeDataT<epicsUInt64>();
  } else if (dataType == NDFloat32) {
    status = computeDataT<epicsFloat32>();
  } else if (dataType == NDFloat64) {
    status = computeDataT<epicsFloat64>();
  } else {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "%s invalid dataType %d.\n", functionName.c_str(), dataType);
  }
  

  return status;
}

template <typename T> asynStatus ADSimPeaks::computeDataT()
{
  asynStatus status = asynSuccess;
  NDArrayInfo_t arrayInfo;

  string functionName(s_className + "::" + __func__);
  
  if (p_NDArray == NULL) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "%s invalid NDArray pointer.\n", functionName.c_str());
  }
  
  p_NDArray->getInfo(&arrayInfo);
  T *pData = static_cast<T*>(p_NDArray->pData);
  
  int integrate = 0;
  getIntegerParam(ADSPIntegrateParam, &integrate);
  if ((integrate == 0) || (m_needReset)) {
    for (int i=0; i<arrayInfo.nElements; i++) {
      pData[i] = 0;
    }
    m_needReset = false;
  }

  epicsInt32 peak_type = 0;
  epicsFloat64 peak_pos = 0.0;
  epicsFloat64 peak_fwhm = 0.0;
  epicsFloat64 peak_max = 0.0;
  epicsFloat64 result = 0.0;
  epicsFloat64 result_max = 0.0;
  epicsFloat64 scale_factor = 0.0;
  
  getIntegerParam(ADSPPeakTypeParam, &peak_type);
  getDoubleParam(ADSPPeakPosParam, &peak_pos);
  getDoubleParam(ADSPPeakFWHMParam, &peak_fwhm);
  getDoubleParam(ADSPPeakMaxParam, &peak_max);

  //Calculate profile and store in a cache
  std::vector<epicsFloat64> cache(arrayInfo.nElements, 0.0);
  for (epicsUInt32 bin=0; bin<cache.size(); bin++) {
    if (peak_type == static_cast<epicsUInt32>(e_peak_type::gaussian)) { 
      computeGaussian(peak_pos, peak_fwhm, bin, &result);
      cache.at(bin) = result;
    } else if (peak_type == static_cast<epicsUInt32>(e_peak_type::lorentz)) { 
      computeLorentz(peak_pos, peak_fwhm, bin, &result);
      cache.at(bin) = result;
    }
    if (result > result_max) {
      result_max = result;
    }
  }
  //Scale profile to the desired height
  if ((result_max > -s_zeroCheck) && (result_max < s_zeroCheck)) {
    scale_factor = 1.0;
  } else {
    scale_factor = peak_max / result_max;
  }
  for (epicsUInt32 bin=0; bin<cache.size(); bin++) {
    result = cache.at(bin) * scale_factor;
    pData[bin] += static_cast<T>(result);
  }
  
  return status;
}


asynStatus ADSimPeaks::computeGaussian(epicsFloat64 pos, epicsFloat64 fwhm, epicsUInt32 bin, epicsFloat64 *result)
{
  asynStatus status = asynSuccess;
  string functionName(s_className + "::" + __func__);

  //TODO - error check and use exceptions
  
  epicsFloat64 sigma = fwhm / (2.0*sqrt(2.0*log(2.0)));
  *result = (1.0 / (sigma*sqrt(2.0*M_PI))) * exp(-(((bin-pos)*(bin-pos))) / (2.0*(sigma*sigma)));

  return status;
}

asynStatus ADSimPeaks::computeLorentz(epicsFloat64 pos, epicsFloat64 fwhm, epicsUInt32 bin, epicsFloat64 *result)
{
  asynStatus status = asynSuccess;
  string functionName(s_className + "::" + __func__);

  //TODO - error check and use exceptions
  
  epicsFloat64 gamma = fwhm / 2.0;
  *result = (1 / (M_PI*gamma)) * ((gamma*gamma) / (((bin-pos)*(bin-pos)) + (gamma*gamma)));

  return status;
}



static void ADSimPeaksTaskC(void *drvPvt)
{
    ADSimPeaks *pPvt = (ADSimPeaks *)drvPvt;

    pPvt->ADSimPeaksTask();
}


extern "C" {

  asynStatus ADSimPeaksConfig(const char *portName, int maxSize, int maxPeaks,
			      int dataType, int maxBuffers, size_t maxMemory,
			      int priority, int stackSize)
  {
    asynStatus status = asynSuccess;
    
    // Instantiate class
    try {
      ADSimPeaks *adsp = new ADSimPeaks(portName, maxSize, maxPeaks,
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
  static const iocshArg ADSimPeaksConfigArg1 = {"Max Size", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg2 = {"Max Peaks", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg3 = {"Data Type", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg4 = {"maxBuffers", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg5 = {"maxMemory", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg6 = {"priority", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg7 = {"stackSize", iocshArgInt};
  static const iocshArg * const ADSimPeaksConfigArgs[] =  {&ADSimPeaksConfigArg0,
							   &ADSimPeaksConfigArg1,
							   &ADSimPeaksConfigArg2,
							   &ADSimPeaksConfigArg3,
							   &ADSimPeaksConfigArg4,
							   &ADSimPeaksConfigArg5,
							   &ADSimPeaksConfigArg6,
							   &ADSimPeaksConfigArg7};
  static const iocshFuncDef configADSimPeaks = {"ADSimPeaksConfig", 8, ADSimPeaksConfigArgs};
  static void configADSimPeaksCallFunc(const iocshArgBuf *args)
  {
    ADSimPeaksConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
		     args[4].ival, args[5].ival, args[6].ival, args[7].ival);
  }
  
  static void ADSimPeaksRegister(void)
  {
    
    iocshRegister(&configADSimPeaks, configADSimPeaksCallFunc);
  }
  
    epicsExportRegistrar(ADSimPeaksRegister);

} // Extern "C"

