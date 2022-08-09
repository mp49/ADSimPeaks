/**
 * \brief areaDetector driver to simulate 1D peaks with background 
 *        profiles and noise. 
 *
 * This areaDetector driver can be used to simulate semi-realistic looking
 * data in 1D. It can produce a 1D NDArray object of variable size and 
 * of different data types. The data can contain a background and any number 
 * of peaks of a few different shapes, with the option to add noise to the 
 * signal.
 *
 * Currently the supported peak shapes are:
 * 1) Gaussian
 * 2) Lorentzian (also known as Cauchy)
 * 3) Voigt (implemented as a psudo-Voigt, which is an approximation)
 *
 * The background is defined as a 3rd order polynomial so that the shape
 * can be a flat offset, a slope or a curve.
 *
 * The noise type can be either uniformly distributed or distributed
 * according to a Gaussian profile. 
 *
 * \author Matt Pearson 
 * \date July 11th, 2022
 *
 */


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

/**
 * Constructor. This creates the driver object and the thread used for
 * generating the data profile.
 *
 * \arg \c portName The Asyn port name
 * \arg \c maxSize The maximum number of bins in the NDArray object 
 * \arg \c maxPeaks The maximum number of peaks (i.e. the Asyn addresses)
 * \arg \c dataType The data type (Uint8, UInt16, etc) to initially use
 * \arg \c maxBuffers The asynPortDriver max buffers (0=unlimited)
 * \arg \c maxMemory The asynPortDriver max memory (0=unlimited)
 * \arg \c priority The asynPortDriver priority (0=default)
 * \arg \c stackSize The asynPortDriver stackSize (0=default)
 *
 */
ADSimPeaks::ADSimPeaks(const char *portName, int maxSize, int maxPeaks,
		       NDDataType_t dataType, int maxBuffers, size_t maxMemory,
		       int priority, int stackSize)
  : ADDriver(portName, maxPeaks, 0, maxBuffers, maxMemory, 0, 0, 0, 1, priority, stackSize),
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
  createParam(ADSPNoiseTypeParamString, asynParamInt32, &ADSPNoiseTypeParam);
  createParam(ADSPNoiseLevelParamString, asynParamFloat64, &ADSPNoiseLevelParam);
  createParam(ADSPElapsedTimeParamString, asynParamFloat64, &ADSPElapsedTimeParam);
  createParam(ADSPPeakTypeParamString, asynParamInt32, &ADSPPeakTypeParam);
  createParam(ADSPPeakPosParamString, asynParamFloat64, &ADSPPeakPosParam);
  createParam(ADSPPeakFWHMParamString, asynParamFloat64, &ADSPPeakFWHMParam);
  createParam(ADSPPeakMaxParamString, asynParamFloat64, &ADSPPeakMaxParam);
  createParam(ADSPBGC0ParamString, asynParamFloat64, &ADSPBGC0Param);
  createParam(ADSPBGC1ParamString, asynParamFloat64, &ADSPBGC1Param);
  createParam(ADSPBGC2ParamString, asynParamFloat64, &ADSPBGC2Param);
  createParam(ADSPBGC3ParamString, asynParamFloat64, &ADSPBGC3Param);
  createParam(ADSPBGSHParamString, asynParamFloat64, &ADSPBGSHParam);
  
  //Initialize non static, non const, data members
  m_acquiring = 0;
  m_uniqueId = 0;
  m_needNewArray = true;
  m_needReset = false;

  //Seed the random number generator
  epicsTimeStamp nowTime;
  epicsTimeGetCurrent(&nowTime);
  m_rand_gen.seed(nowTime.secPastEpoch);

  bool paramStatus = true;
  //Initialise any paramLib parameters that need passing up to device support
  paramStatus = ((setIntegerParam(ADAcquire, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADStatus, ADStatusIdle) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADAcquirePeriod, 1.0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADMaxSizeX, m_maxSize) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSizeX, m_maxSize) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSPIntegrateParam, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSPNoiseTypeParam, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPNoiseLevelParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPElapsedTimeParam, 0.0) == asynSuccess) && paramStatus);
  //Peak Params
  for (epicsUInt32 peak=0; peak<m_maxPeaks; peak++) {
    paramStatus = ((setIntegerParam(ADSPPeakTypeParam, 0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakPosParam, 1.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakFWHMParam, 1.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakMaxParam, 1.0) == asynSuccess) && paramStatus);
    callParamCallbacks(peak);
  }
  //Background Params
  paramStatus = ((setDoubleParam(ADSPBGC0Param, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC1Param, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC2Param, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC3Param, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGSHParam, 0.0) == asynSuccess) && paramStatus);
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

/**
 * Destructor. 
 * We should never get here (unless we have an exit handler calling this)
 */
ADSimPeaks::~ADSimPeaks()
{
  string functionName(s_className + "::" + __func__);
  cout << functionName << " exiting. " << endl;
}

/**
 * Implementation of writeInt32. This is called when writing 
 * integer values.
 *
 * /arg /c pasynUser Pointer to the asynUser.
 * /arg /c value The value to write to the parameter library.
 *
 * /return /c asynStatus
 */
asynStatus ADSimPeaks::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  asynStatus status = asynSuccess;
  int imageMode = 0;
  int addr = 0;
  int function = pasynUser->reason;
  
  string functionName(s_className + "::" + __func__);
  
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s entry...\n", functionName.c_str());

  //Read address (ie. peak number).
  status = getAddress(pasynUser, &addr); 
  if (status != asynSuccess) {
    return(status);
  }
  
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
    value = std::max(1, std::min(value, static_cast<int32_t>(m_maxSize)));
    int currentXSize = 0;
    getIntegerParam(ADSizeX, &currentXSize);
    if (value != currentXSize) {
      m_needNewArray = true;
    }
  } else if (function == NDDataType) {
    m_needNewArray = true;  
  } else if (function == ADNumImages) {
    value = std::max(1, value);
  }
  

  if (status != asynSuccess) {
    callParamCallbacks();
    return asynError;
  }

  status = (asynStatus) setIntegerParam(addr, function, value);
  if (status != asynSuccess) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s error setting parameter. asynUser->reason: %d, value: %d\n", 
              functionName.c_str(), function, value);
    return(status);
  }
 
  callParamCallbacks(addr);

  return status;

}

/**
 * Implementation of writeFloat64. This is called when writing 
 * double values.
 *
 * /arg /c pasynUser Pointer to the asynUser.
 * /arg /c value The value to write to the parameter library.
 *
 * /return /c asynStatus
 */
asynStatus ADSimPeaks::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
  asynStatus status = asynSuccess;
  int addr = 0;
  int function = pasynUser->reason;

  string functionName(s_className + "::" + __func__);
  
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s entry...\n", functionName.c_str());

  //Read address (ie. peak number).
  status = getAddress(pasynUser, &addr); 
  if (status != asynSuccess) {
    return(status);
  }

  if (function == ADAcquirePeriod) {
     value = std::max(0.0, value);
  } else if (function == ADSPPeakFWHMParam) {
     value = std::max(1.0, value);
  }
  
  if (status != asynSuccess) {
    callParamCallbacks();
    return asynError;
  }

  status = (asynStatus) setDoubleParam(addr, function, value);
  if (status != asynSuccess) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s error setting parameter. asynUser->reason: %d, value: %f\n", 
              functionName.c_str(), function, value);
    return(status);
  }
 
  callParamCallbacks(addr);

  return status;

}

/**
 * Implementation of the standard report function.
 * This prints the driver configuration.
 */
void ADSimPeaks::report(FILE *fp, int details)
{
  epicsInt32 intParam = 0;
  epicsFloat64 floatParam = 0.0;
  
  string functionName(s_className + "::" + __func__);
  fprintf(fp, "%s. portName: %s\n", functionName.c_str(), this->portName);
  
  if (details > 0) {
    fprintf(fp, " Internal Data:\n");
    fprintf(fp, "  m_acquiring: %d\n", m_acquiring);
    fprintf(fp, "  m_maxSize: %d\n", m_maxSize);
    fprintf(fp, "  m_maxPeaks: %d\n", m_maxPeaks);
    fprintf(fp, "  m_uniqueId: %d\n", m_uniqueId);
    fprintf(fp, "  m_needNewArray: %d\n", m_needNewArray);
    fprintf(fp, "  m_needReset: %d\n", m_needReset);

    fprintf(fp, " Simulation State:\n");
    getIntegerParam(ADAcquire, &intParam);
    fprintf(fp, "  acquire: %d\n", intParam);
    getIntegerParam(ADSizeX, &intParam);
    fprintf(fp, "  NDArray size: %d\n", intParam);
    getIntegerParam(NDDataType, &intParam);
    fprintf(fp, "  NDArray data type: %d\n", intParam);
    getIntegerParam(ADImageMode, &intParam);
    fprintf(fp, "  image mode: %d\n", intParam);
    getIntegerParam(ADNumImages, &intParam);
    fprintf(fp, "  num images: %d\n", intParam);

    getIntegerParam(ADSPIntegrateParam, &intParam);
    fprintf(fp, "  integrate: %d\n", intParam);
    getDoubleParam(ADSPElapsedTimeParam, &floatParam);
    fprintf(fp, "  elapsed time: %f\n", floatParam);

    getIntegerParam(ADSPNoiseTypeParam, &intParam);
    fprintf(fp, "  noise type: %d\n", intParam);
    getDoubleParam(ADSPNoiseLevelParam, &floatParam);
    fprintf(fp, "  noise level: %f\n", floatParam);

    getDoubleParam(ADSPBGC0Param, &floatParam);
    fprintf(fp, "  background coefficient 0: %f\n", floatParam);
    getDoubleParam(ADSPBGC1Param, &floatParam);
    fprintf(fp, "  background coefficient 1: %f\n", floatParam);
    getDoubleParam(ADSPBGC2Param, &floatParam);
    fprintf(fp, "  background coefficient 2: %f\n", floatParam);
    getDoubleParam(ADSPBGC3Param, &floatParam);
    fprintf(fp, "  background coefficient 3: %f\n", floatParam);
    getDoubleParam(ADSPBGSHParam, &floatParam);
    fprintf(fp, "  background shift: %f\n", floatParam);

    fprintf(fp, " Peak Information:\n");
    for (epicsUInt32 i=0; i<m_maxPeaks; i++) {
      fprintf(fp, "  peak: %d\n", i);
      getIntegerParam(i, ADSPPeakTypeParam, &intParam);
      if (intParam == static_cast<epicsUInt32>(e_peak_type::none)) {
	fprintf(fp, "   none (disabled)\n");
      } else {
	fprintf(fp, "   type: %d\n", intParam);
	getDoubleParam(i, ADSPPeakPosParam, &floatParam);
	fprintf(fp, "   position: %f\n", floatParam);
	getDoubleParam(i, ADSPPeakFWHMParam, &floatParam);
	fprintf(fp, "   fwhm: %f\n", floatParam);
	getDoubleParam(i, ADSPPeakMaxParam, &floatParam);
	fprintf(fp, "   max: %f\n", floatParam);
      }
    }
  }
  // Invoke the base class method.
  // This will by default print the addr=0 parameters.
  ADDriver::report(fp, details);
}

/**
 * Return the state of the driver initialization 
 * (i.e., did the constructor complete normally)
 *
 * /return /c boolean (true means initialized OK)
 */
bool ADSimPeaks::getInitialized(void)
{
  return m_initialized;
}

/**
 * The data simulation thread which runs forever.
 */
void ADSimPeaks::ADSimPeaksTask(void)
{
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
  int imageMode = 0;
  int numImages = 0;
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

      getIntegerParam(ADImageMode, &imageMode);
      getIntegerParam(ADNumImages, &numImages);
      
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

      //Figure out if we are finished
      if ((imageMode == ADImageSingle) || ((imageMode == ADImageMultiple) && (imagesCounter >= numImages))) {
	m_acquiring = false;
	setIntegerParam(ADStatus, ADStatusIdle);
	setStringParam(ADStatusMessage, "Simulation Idle");
      } else {
	//Wait for a stop event
	this->unlock();
	eventStatus = epicsEventWaitWithTimeout(m_stopEvent, updatePeriod);
	this->lock();
	if (eventStatus == epicsEventWaitOK) {
	  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
		    "%s stopping simulation.\n", functionName.c_str());
	  m_acquiring = false;
	  setStringParam(ADStatusMessage, "Simulation Idle");
	}
      }
      callParamCallbacks();
    }
    
  }
  
}

/**
 * Generate a simulation array using the input data type
 *
 * /arg /c dataType The NDDataType to use (eg. NDInt32, NDFloat64, etc.)
 *
 * /return /c asynStatus 
 */
asynStatus ADSimPeaks::computeData(NDDataType_t dataType)
{
  asynStatus status = asynSuccess;

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

/**
 * Templated version of ADSimPeaks::computeData. This does the actual work and 
 * populates the NDArray object. The background profile is first calculated, 
 * then we add in the desired peaks, then we modify the resulting profile 
 * with optional noise.
 *
 * /return /c asynStatus 
 */
template <typename T> asynStatus ADSimPeaks::computeDataT()
{
  asynStatus status = asynSuccess;
  NDArrayInfo_t arrayInfo;
  epicsUInt32 size = 0;
  epicsInt32 peak_type = 0;
  epicsFloat64 peak_pos = 0.0;
  epicsFloat64 peak_fwhm = 0.0;
  epicsFloat64 peak_max = 0.0;
  epicsFloat64 result = 0.0;
  epicsFloat64 result_max = 0.0;
  epicsFloat64 scale_factor = 0.0;
  epicsFloat64 bg_c0 = 0.0;
  epicsFloat64 bg_c1 = 0.0;
  epicsFloat64 bg_c2 = 0.0;
  epicsFloat64 bg_c3 = 0.0;
  epicsFloat64 bg_sh = 0.0;
  epicsInt32 noise_type = 0;
  epicsFloat64 noise_level = 0.0;
  epicsFloat64 noise = 0.0;

  string functionName(s_className + "::" + __func__);
  
  if (p_NDArray == NULL) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "%s invalid NDArray pointer.\n", functionName.c_str());
    return asynError;
  }
  
  p_NDArray->getInfo(&arrayInfo);
  T *pData = static_cast<T*>(p_NDArray->pData);
  size = arrayInfo.nElements;
  
  //Reset the array data if we need to
  int integrate = 0;
  getIntegerParam(ADSPIntegrateParam, &integrate);
  if ((integrate == 0) || (m_needReset)) {
    for (epicsUInt32 i=0; i<arrayInfo.nElements; i++) {
      pData[i] = 0;
    }
    m_needReset = false;
  }

  //Calculate the background profile
  getDoubleParam(ADSPBGC0Param, &bg_c0);
  getDoubleParam(ADSPBGC1Param, &bg_c1);
  getDoubleParam(ADSPBGC2Param, &bg_c2);
  getDoubleParam(ADSPBGC3Param, &bg_c3);
  getDoubleParam(ADSPBGSHParam, &bg_sh);
  for (epicsInt32 bin=0; bin<static_cast<epicsInt32>(size); bin++) {
    pData[bin] += bg_c0 + (bin-bg_sh)*bg_c1 + pow((bin-bg_sh),2)*bg_c2 + pow((bin-bg_sh),3)*bg_c3;
  }
  
  //Calculate the peak profile and scale it to the desired height
  for (epicsUInt32 peak=0; peak<m_maxPeaks; peak++) {
    getIntegerParam(peak, ADSPPeakTypeParam, &peak_type);
    getDoubleParam(peak, ADSPPeakPosParam, &peak_pos);
    getDoubleParam(peak, ADSPPeakFWHMParam, &peak_fwhm);
    getDoubleParam(peak, ADSPPeakMaxParam, &peak_max);
    if (peak_type == static_cast<epicsUInt32>(e_peak_type::gaussian)) {
      computeGaussian(peak_pos, peak_fwhm, peak_pos, &result_max);
      scale_factor = peak_max / zeroCheck(result_max);
      for (epicsUInt32 bin=0; bin<size; bin++) {
	computeGaussian(peak_pos, peak_fwhm, bin, &result);
	result = (result*scale_factor);
	pData[bin] += static_cast<T>(result);
      }
    } else if (peak_type == static_cast<epicsUInt32>(e_peak_type::lorentz)) {
      computeLorentz(peak_pos, peak_fwhm, peak_pos, &result_max);
      scale_factor = peak_max / zeroCheck(result_max);
      for (epicsUInt32 bin=0; bin<size; bin++) {
	computeLorentz(peak_pos, peak_fwhm, bin, &result);
	result = (result*scale_factor);
	pData[bin] += static_cast<T>(result);
      }
    } else if (peak_type == static_cast<epicsUInt32>(e_peak_type::pseudovoigt)) {
      computePseudoVoigt(peak_pos, peak_fwhm, peak_pos, &result_max);
      scale_factor = peak_max / zeroCheck(result_max);
      for (epicsUInt32 bin=0; bin<size; bin++) {
	computePseudoVoigt(peak_pos, peak_fwhm, bin, &result);
	result = (result*scale_factor);
	pData[bin] += static_cast<T>(result);
      }
    }
  }

  //Generate noise
  getIntegerParam(ADSPNoiseTypeParam, &noise_type);
  getDoubleParam(ADSPNoiseLevelParam, &noise_level);
  if (noise_type == static_cast<epicsUInt32>(e_noise_type::uniform)) {
    std::uniform_real_distribution<double> dist(-1.0,1.0);
    for (epicsUInt32 bin=0; bin<size; bin++) {
      noise = dist(m_rand_gen);
      noise = noise_level * noise;
      pData[bin] += static_cast<T>(noise);
    }
  } else if (noise_type == static_cast<epicsUInt32>(e_noise_type::gaussian)) {
    std::normal_distribution<double> dist(0.0,1.0);
    for (epicsUInt32 bin=0; bin<size; bin++) {
      noise = dist(m_rand_gen);
      noise = noise_level * noise;
      pData[bin] += static_cast<T>(noise);
    }
  }
  
  return status;
}

/**
 * Utility function to check if a floating point number is close to zero.
 *
 * /arg /c value The value to check 
 *
 * /return The original input value or 1.0 (if it was too close to zero)
 */
epicsFloat64 ADSimPeaks::zeroCheck(epicsFloat64 value)
{
  if ((value > -s_zeroCheck) && (value < s_zeroCheck)) {
    return 1.0;
  } else {
    return value;
  }
}
 

/**
 * Implementation of a Gaussian function which has center 'pos' and full width half max 'fwhm'.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Normal_distribution
 * https://en.wikipedia.org/wiki/Gaussian_function
 *
 * /arg /c pos The center of the distribution
 * /arg /c fwhm The FWHM of the distribution
 * /arg /c bin The position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeGaussian(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result)
{
  asynStatus status = asynSuccess;
  string functionName(s_className + "::" + __func__);

  if (fwhm < 1.0) {
    fwhm = 1.0;
  }
  
  epicsFloat64 sigma = fwhm / (2.0*sqrt(2.0*log(2.0)));
  *result = (1.0 / (sigma*sqrt(2.0*M_PI))) * exp(-(((bin-pos)*(bin-pos))) / (2.0*(sigma*sigma)));

  return status;
}

/**
 * Implementation of a Cauchy-Lorentz function which has center 'pos' and full width half max 'fwhm'.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Cauchy_distribution
 *
 * /arg /c pos The center of the distribution
 * /arg /c fwhm The FWHM of the distribution
 * /arg /c bin The position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeLorentz(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result)
{
  asynStatus status = asynSuccess;
  string functionName(s_className + "::" + __func__);

  //TODO - error check and use exceptions

  
  if (fwhm < 1.0) {
    fwhm = 1.0;
  }
  
  epicsFloat64 gamma = fwhm / 2.0;
  *result = (1 / (M_PI*gamma)) * ((gamma*gamma) / (((bin-pos)*(bin-pos)) + (gamma*gamma)));

  return status;
}

/**
 * Implementation of the approximation of the Voigt function (known as the Psudo-Voigt) 
 * which has center 'pos' and full width half max 'fwhm'.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Voigt_profile
 *
 * /arg /c pos The center of the distribution
 * /arg /c fwhm The FWHM of the distribution
 * /arg /c bin The position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computePseudoVoigt(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result)
{
  asynStatus status = asynSuccess;

  epicsFloat64 fwhm_g = 0.0;
  epicsFloat64 fwhm_l = 0.0;
  epicsFloat64 fwhm_sum = 0.0;
  epicsFloat64 fwhm_tot = 0.0;
  epicsFloat64 eta = 0.0;
  epicsFloat64 gaussian = 0.0;
  epicsFloat64 lorentz = 0.0;
  
  string functionName(s_className + "::" + __func__);

  //TODO - error check and use exceptions

  if (fwhm < 1.0) {
    fwhm = 1.0;
  }
  
  //This implementation assumes the FWHM of the Gaussian and Lorentz is the same. However, we
  //still use the full approximation for the Pseudo-Voigt total FWHM (fwhm_tot) and use two FWHM parameters
  //(fwhm_g and fwhm_l), so that this function can easily be modified to use a different Gaussian
  //and Lorentzian FWHM.

  epicsFloat64 p1 = 2.69269;
  epicsFloat64 p2 = 2.42843;
  epicsFloat64 p3 = 4.47163;
  epicsFloat64 p4 = 0.07842;

  epicsFloat64 e1 = 1.36603;
  epicsFloat64 e2 = 0.47719;
  epicsFloat64 e3 = 0.11116;
  
  fwhm_g = fwhm;
  fwhm_l = fwhm;
  fwhm_sum = pow(fwhm_g,5) + (p1*pow(fwhm_g,4)*fwhm_l) + (p2*pow(fwhm_g,3)*pow(fwhm_l,2)) +
            (p3*pow(fwhm_g,2)*pow(fwhm_l,3)) + (p4*fwhm_g*pow(fwhm_l,4)) + pow(fwhm_l,5);
  fwhm_tot = pow(fwhm_sum,0.2);
  
  eta = ((e1*(fwhm_l/fwhm_tot)) - (e2*pow((fwhm_l/fwhm_tot),2)) + (e3*pow((fwhm_l/fwhm_tot),3)));
  
  computeGaussian(pos, fwhm, bin, &gaussian);
  computeLorentz(pos, fwhm, bin, &lorentz);

  *result = ((1.0 - eta)*gaussian) + (eta*lorentz);

  return status;
}

/**
 * Implementation of a bivariate Gaussian function which has center (x,y) and full width half max 
 * 'x_fwhm' and 'y_fwhm', with X and Y correlation rho (where -1<=rho<=1).
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Normal_distribution
 * https://en.wikipedia.org/wiki/Gaussian_function
 *
 * /arg /c x_pos The X coordinate of the distribution
 * /arg /c y_pos The Y coordinate of the distribution
 * /arg /c x_fwhm The X dimension FWHM of the distribution
 * /arg /c y_fwhm The Y dimension FWHM of the distribution
 * /arg /c x_bin The X position to use for the function
 * /arg /c y_bin The Y position to use for the function
 * /arg /c rho The X/Y correlation
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeGaussian2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
					 epicsFloat64 x_fwhm, epicsFloat64 y_fwhm,
					 epicsInt32 x_bin, epicsInt32 y_bin,
					 epicsFloat64 rho, epicsFloat64 *result)
{
  asynStatus status = asynSuccess;
  string functionName(s_className + "::" + __func__);

  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);
  rho = std::min(1.0, std::max(-1.0, rho));
  
  epicsFloat64 x_sig = x_fwhm / (2.0*sqrt(2.0*log(2.0)));
  epicsFloat64 y_sig = y_fwhm / (2.0*sqrt(2.0*log(2.0)));

  epicsFloat64 xy_amp = 1.0 / (2.0 * M_PI * x_sig * y_sig * sqrt(1-(rho*rho)));
  epicsFloat64 xy_factor = -1 / (2*(1-(rho*rho)));
  epicsFloat64 xy_calc1 = (x_bin-x_pos)/x_sig;
  epicsFloat64 xy_calc2 = (y_bin-y_pos)/y_sig;
    
  *result = xy_amp * exp(xy_factor*(xy_calc1*xy_calc1 - 2*rho*xy_calc1*xy_calc2 + xy_calc2*xy_calc2));

  return status;
}

/**
 * Implementation of a bivariate Cauchy-Lorentz function which has center (x,y) and 
 * full width half max 'fwhm'.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Cauchy_distribution
 *
 * I can only find bivariate Cauchy functions that are symmetric in X and Y so we just use
 * a single FWHM argument and there is no covariance factor.
 * 
 * /arg /c x_pos The X coordinate of the distribution
 * /arg /c y_pos The Y coordinate of the distribution
 * /arg /c fwhm The FWHM of the distribution
 * /arg /c x_bin The X position to use for the function
 * /arg /c y_bin The Y position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeLorentz2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
					epicsFloat64 fwhm, epicsInt32 x_bin,
					epicsInt32 y_bin, epicsFloat64 *result)
{
  asynStatus status = asynSuccess;
  string functionName(s_className + "::" + __func__);
  
  fwhm = std::max(1.0, fwhm);
  
  epicsFloat64 gamma = fwhm / 2.0;
  epicsFloat64 xy_calc1 = x_bin-x_pos;
  epicsFloat64 xy_calc2 = y_bin-y_pos;
  
  *result = (1 / (2*M_PI)) * (gamma / pow(((xy_calc1*xy_calc1) + (xy_calc2*xy_calc2) + gamma*gamma),1.5));

  return status;
}


/**
 * C function to tie into EPICS
 */
static void ADSimPeaksTaskC(void *drvPvt)
{
    ADSimPeaks *pPvt = (ADSimPeaks *)drvPvt;

    pPvt->ADSimPeaksTask();
}

/**
 * C linkage functions to define the shell commands
 */
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

