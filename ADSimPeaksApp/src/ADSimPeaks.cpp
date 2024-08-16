/**
 * \brief areaDetector driver to simulate 1D and 2D peaks with background 
 *        profiles and noise. 
 *
 * This areaDetector driver can be used to simulate semi-realistic diffraction
 * data in 1D and 2D. It can produce a 1D or 2D NDArray object of variable size and 
 * of different data types. The data can contain a polynomial background and any number 
 * of peaks of a few different shapes, with the option to add different kinds of 
 * noise to the signal.
 *
 * The background type can either be a 3rd order polynomial, so that the shape can be 
 * a flat offset, a slope or a curve, or an exponential with a slope and offset. 
 *
 * The noise type can be either uniformly distributed or distributed
 * according to a Gaussian profile. 
 *
 * The width of the peaks can be restricted by setting hard lower and upper
 * boundaries, which may be useful in some cases (such as saving CPU). 
 * Some types of peaks have wide tails and so this may be of limited use 
 * for those. However, using a boundary is one way of simulating an edge. 
 *
 * There are other classes defined in other files that are used by ADSimPeaks:
 * ADSimPeaksPeak - contains the implementation of the various peak shapes
 * ADSimPeaksData - container class to hold peak information
 * 
 * \author Matt Pearson 
 * \date Aug 31st, 2022 
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

// Static Data
// Class name
const string ADSimPeaks::s_className = "ADSimPeaks";
// Constant used to test for 0.0
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
ADSimPeaks::ADSimPeaks(const char *portName, int maxSizeX, int maxSizeY, int maxPeaks,
		       NDDataType_t dataType, int maxBuffers, size_t maxMemory,
		       int priority, int stackSize)
  : ADDriver(portName, maxPeaks, 0, maxBuffers, maxMemory, 0, 0, 0, 1, priority, stackSize),
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
  createParam(ADSPNoiseTypeParamString, asynParamInt32, &ADSPNoiseTypeParam);
  createParam(ADSPNoiseLevelParamString, asynParamFloat64, &ADSPNoiseLevelParam);
  createParam(ADSPNoiseClampParamString, asynParamInt32, &ADSPNoiseClampParam);
  createParam(ADSPNoiseLowerParamString, asynParamFloat64, &ADSPNoiseLowerParam);
  createParam(ADSPNoiseUpperParamString, asynParamFloat64, &ADSPNoiseUpperParam);
  createParam(ADSPElapsedTimeParamString, asynParamFloat64, &ADSPElapsedTimeParam);
  createParam(ADSPPeakType1DParamString, asynParamInt32, &ADSPPeakType1DParam);
  createParam(ADSPPeakType2DParamString, asynParamInt32, &ADSPPeakType2DParam);
  createParam(ADSPPeakPosXParamString, asynParamFloat64, &ADSPPeakPosXParam);
  createParam(ADSPPeakPosYParamString, asynParamFloat64, &ADSPPeakPosYParam);
  createParam(ADSPPeakFWHMXParamString, asynParamFloat64, &ADSPPeakFWHMXParam);
  createParam(ADSPPeakFWHMYParamString, asynParamFloat64, &ADSPPeakFWHMYParam);
  createParam(ADSPPeakAmpParamString, asynParamFloat64, &ADSPPeakAmpParam);
  createParam(ADSPPeakCorParamString, asynParamFloat64, &ADSPPeakCorParam);
  createParam(ADSPPeakP1ParamString, asynParamFloat64, &ADSPPeakP1Param);
  createParam(ADSPPeakP2ParamString, asynParamFloat64, &ADSPPeakP2Param);
  createParam(ADSPPeakMinXParamString, asynParamInt32, &ADSPPeakMinXParam);
  createParam(ADSPPeakMinYParamString, asynParamInt32, &ADSPPeakMinYParam);
  createParam(ADSPPeakMaxXParamString, asynParamInt32, &ADSPPeakMaxXParam);
  createParam(ADSPPeakMaxYParamString, asynParamInt32, &ADSPPeakMaxYParam);
  createParam(ADSPBGTypeXParamString, asynParamInt32, &ADSPBGTypeXParam);
  createParam(ADSPBGC0XParamString, asynParamFloat64, &ADSPBGC0XParam);
  createParam(ADSPBGC1XParamString, asynParamFloat64, &ADSPBGC1XParam);
  createParam(ADSPBGC2XParamString, asynParamFloat64, &ADSPBGC2XParam);
  createParam(ADSPBGC3XParamString, asynParamFloat64, &ADSPBGC3XParam);
  createParam(ADSPBGSHXParamString, asynParamFloat64, &ADSPBGSHXParam);
  createParam(ADSPBGTypeYParamString, asynParamInt32, &ADSPBGTypeYParam);
  createParam(ADSPBGC0YParamString, asynParamFloat64, &ADSPBGC0YParam);
  createParam(ADSPBGC1YParamString, asynParamFloat64, &ADSPBGC1YParam);
  createParam(ADSPBGC2YParamString, asynParamFloat64, &ADSPBGC2YParam);
  createParam(ADSPBGC3YParamString, asynParamFloat64, &ADSPBGC3YParam);
  createParam(ADSPBGSHYParamString, asynParamFloat64, &ADSPBGSHYParam);
  
  //Initialize non static, non const, data members
  m_acquiring = 0;
  m_uniqueId = 0;
  m_needNewArray = true;
  m_needReset = false;
  m_2d = false;
  if (m_maxSizeY > 0) {
    m_2d = true;
  }

  //Seed the random number generator
  epicsTimeStamp nowTime;
  epicsTimeGetCurrent(&nowTime);
  m_rand_gen.seed(nowTime.secPastEpoch);

  bool paramStatus = true;
  //Initialise any paramLib parameters that need passing up to device support
  paramStatus = ((setIntegerParam(ADAcquire, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADStatus, ADStatusIdle) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADAcquirePeriod, 1.0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADMaxSizeX, m_maxSizeX) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADMaxSizeY, m_maxSizeY) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSizeX, m_maxSizeX) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSizeY, m_maxSizeY) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSPIntegrateParam, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSPNoiseTypeParam, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPNoiseLevelParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setIntegerParam(ADSPNoiseClampParam, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPNoiseLowerParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPNoiseUpperParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPElapsedTimeParam, 0.0) == asynSuccess) && paramStatus);
  //Peak Params
  for (epicsUInt32 peak=0; peak<m_maxPeaks; peak++) {
    paramStatus = ((setIntegerParam(ADSPPeakType1DParam, 0) == asynSuccess) && paramStatus);
    paramStatus = ((setIntegerParam(ADSPPeakType2DParam, 0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakPosXParam, 1.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakPosYParam, 1.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakFWHMXParam, 1.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakFWHMYParam, 1.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakAmpParam, 1.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakCorParam, 1.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakP1Param, 0.0) == asynSuccess) && paramStatus);
    paramStatus = ((setDoubleParam(ADSPPeakP2Param, 0.0) == asynSuccess) && paramStatus);
    paramStatus = ((setIntegerParam(ADSPPeakMinXParam, 0) == asynSuccess) && paramStatus);
    paramStatus = ((setIntegerParam(ADSPPeakMinYParam, 0) == asynSuccess) && paramStatus);
    paramStatus = ((setIntegerParam(ADSPPeakMaxXParam, 0) == asynSuccess) && paramStatus);
    paramStatus = ((setIntegerParam(ADSPPeakMaxYParam, 0) == asynSuccess) && paramStatus);
    callParamCallbacks(peak);
  }
  //Background Params X
  paramStatus = ((setIntegerParam(ADSPBGTypeXParam, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC0XParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC1XParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC2XParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC3XParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGSHXParam, 0.0) == asynSuccess) && paramStatus);
  //Background Params Y
  paramStatus = ((setIntegerParam(ADSPBGTypeYParam, 0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC0YParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC1YParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC2YParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC3YParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGSHYParam, 0.0) == asynSuccess) && paramStatus);
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
  if (!m_2d) {
    cout << functionName << " configured for 1D data" << endl;
  } else {
    cout << functionName << " configured for 2D data" << endl;
  }

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
    value = std::max(1, std::min(value, static_cast<int32_t>(m_maxSizeX)));
    int currentXSize = 0;
    getIntegerParam(ADSizeX, &currentXSize);
    if (value != currentXSize) {
      m_needNewArray = true;
    }
  } else if (function == ADSizeY) {
    value = std::max(1, std::min(value, static_cast<int32_t>(m_maxSizeY)));
    int currentYSize = 0;
    getIntegerParam(ADSizeY, &currentYSize);
    if (value != currentYSize) {
      m_needNewArray = true;
    }
  } else if (function == ADSPPeakMinXParam) {
    value = std::max(0, std::min(value, static_cast<int32_t>(m_maxSizeX-1)));
  } else if (function == ADSPPeakMinYParam) {
    value = std::max(0, std::min(value, static_cast<int32_t>(m_maxSizeY-1)));
  } else if (function == ADSPPeakMaxXParam) {
    value = std::max(0, std::min(value, static_cast<int32_t>(m_maxSizeX-1)));
  } else if (function == ADSPPeakMaxYParam) {
    value = std::max(0, std::min(value, static_cast<int32_t>(m_maxSizeX-1)));
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
  } else if (function == ADSPPeakFWHMXParam) {
    value = std::max(1.0, value);
  } else if (function == ADSPPeakFWHMYParam) {
    value = std::max(1.0, value);
  } else if (function == ADSPPeakCorParam) {
    value = std::min(1.0, std::max(-1.0, value));
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
    fprintf(fp, "  m_maxSizeX: %d\n", m_maxSizeX);
    fprintf(fp, "  m_maxSizeY: %d\n", m_maxSizeY);
    fprintf(fp, "  m_maxPeaks: %d\n", m_maxPeaks);
    fprintf(fp, "  m_uniqueId: %d\n", m_uniqueId);
    fprintf(fp, "  m_needNewArray: %d\n", m_needNewArray);
    fprintf(fp, "  m_needReset: %d\n", m_needReset);
    fprintf(fp, "  m_2d: %d\n", m_2d);

    fprintf(fp, " Simulation State:\n");
    getIntegerParam(ADAcquire, &intParam);
    fprintf(fp, "  acquire: %d\n", intParam);
    getIntegerParam(ADSizeX, &intParam);
    fprintf(fp, "  NDArray size X: %d\n", intParam);
    getIntegerParam(ADSizeY, &intParam);
    fprintf(fp, "  NDArray size Y: %d\n", intParam);
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
    getDoubleParam(ADSPNoiseLowerParam, &floatParam);
    fprintf(fp, "  noise lower: %f\n", floatParam);
    getDoubleParam(ADSPNoiseUpperParam, &floatParam);
    fprintf(fp, "  noise upper: %f\n", floatParam);

    getIntegerParam(ADSPBGTypeXParam, &intParam);
    fprintf(fp, "  background X type: %d\n", intParam);
    getDoubleParam(ADSPBGC0XParam, &floatParam);
    fprintf(fp, "  background X coefficient 0: %f\n", floatParam);
    getDoubleParam(ADSPBGC1XParam, &floatParam);
    fprintf(fp, "  background X coefficient 1: %f\n", floatParam);
    getDoubleParam(ADSPBGC2XParam, &floatParam);
    fprintf(fp, "  background X coefficient 2: %f\n", floatParam);
    getDoubleParam(ADSPBGC3XParam, &floatParam);
    fprintf(fp, "  background X coefficient 3: %f\n", floatParam);
    getDoubleParam(ADSPBGSHXParam, &floatParam);
    fprintf(fp, "  background X shift: %f\n", floatParam);
    if (m_2d) {
      getIntegerParam(ADSPBGTypeYParam, &intParam);
      fprintf(fp, "  background Y type: %d\n", intParam);
      getDoubleParam(ADSPBGC0YParam, &floatParam);
      fprintf(fp, "  background Y coefficient 0: %f\n", floatParam);
      getDoubleParam(ADSPBGC1YParam, &floatParam);
      fprintf(fp, "  background Y coefficient 1: %f\n", floatParam);
      getDoubleParam(ADSPBGC2YParam, &floatParam);
      fprintf(fp, "  background Y coefficient 2: %f\n", floatParam);
      getDoubleParam(ADSPBGC3YParam, &floatParam);
      fprintf(fp, "  background Y coefficient 3: %f\n", floatParam);
      getDoubleParam(ADSPBGSHYParam, &floatParam);
      fprintf(fp, "  background Y shift: %f\n", floatParam);
    } 
    
    fprintf(fp, " Peak Information:\n");
    for (epicsUInt32 i=0; i<m_maxPeaks; i++) {
      fprintf(fp, "  peak: %d\n", i);
      if (!m_2d) {
	getIntegerParam(i, ADSPPeakType1DParam, &intParam);
	if (intParam == static_cast<epicsUInt32>(m_peaks.e_type_1d::none)) {
	  fprintf(fp, "   none (disabled)\n");
	}
      } else {
	getIntegerParam(i, ADSPPeakType2DParam, &intParam);
	if (intParam == static_cast<epicsUInt32>(m_peaks.e_type_2d::none)) {
	  fprintf(fp, "   none (disabled)\n");
	}
      }
      fprintf(fp, "   type: %d\n", intParam);
      getDoubleParam(i, ADSPPeakPosXParam, &floatParam);
      fprintf(fp, "   position X: %f\n", floatParam);
      getDoubleParam(i, ADSPPeakPosYParam, &floatParam);
      fprintf(fp, "   position Y: %f\n", floatParam);
      getDoubleParam(i, ADSPPeakFWHMXParam, &floatParam);
      fprintf(fp, "   fwhm X: %f\n", floatParam);
      getDoubleParam(i, ADSPPeakFWHMYParam, &floatParam);
      fprintf(fp, "   fwhm Y: %f\n", floatParam);
      getDoubleParam(i, ADSPPeakAmpParam, &floatParam);
      fprintf(fp, "   amplitude: %f\n", floatParam);
      getDoubleParam(i, ADSPPeakCorParam, &floatParam);
      fprintf(fp, "   xy correlation: %f\n", floatParam);
      getDoubleParam(i, ADSPPeakP1Param, &floatParam);
      fprintf(fp, "   param 1: %f\n", floatParam);
      getDoubleParam(i, ADSPPeakP2Param, &floatParam);
      fprintf(fp, "   param 2: %f\n", floatParam);
      getIntegerParam(i, ADSPPeakMinXParam, &intParam);
      fprintf(fp, "   min X: %d\n", intParam);
      getIntegerParam(i, ADSPPeakMinYParam, &intParam);
      fprintf(fp, "   min Y: %d\n", intParam);
      getIntegerParam(i, ADSPPeakMaxXParam, &intParam);
      fprintf(fp, "   max X: %d\n", intParam);
      getIntegerParam(i, ADSPPeakMaxYParam, &intParam);
      fprintf(fp, "   max Y: %d\n", intParam);
    } // end of peak loop
  } // end of if (details > 0)

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
  int sizeX = 0;
  int sizeY = 0;
  int ndims=0;
  size_t dims[2] = {0};
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
      getIntegerParam(ADSizeX, &sizeX);
      getIntegerParam(ADSizeY, &sizeY);

      if (!m_2d) {
	ndims = 1;
	dims[0] = sizeX;
	dims[1] = 0;
      } else {
	ndims = 2;
	dims[0] = sizeX;
	dims[1] = sizeY;
      }
      
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
	setIntegerParam(NDArraySizeY, dims[1]);
	setIntegerParam(NDArrayCounter, arrayCounter);
	setIntegerParam(ADNumImagesCounter, imagesCounter);
	
	this->getAttributes(p_NDArray->pAttributeList);
	
	if (arrayCallbacks) {	  
	  // Copy the data to a new NDArray (p_NDArrayPlugins) for use
	  // by the plugins, as we need to hold to our NDArray (p_NDArray)
	  // for integrating data.
	  p_NDArrayPlugins = this->pNDArrayPool->copy(p_NDArray, NULL, true);
	  doCallbacksGenericPointer(p_NDArrayPlugins, NDArrayData, 0);
	  p_NDArrayPlugins->release();
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
        callParamCallbacks();
        setIntegerParam(ADAcquire, 0);
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
		    "%s completed simulation.\n", functionName.c_str());
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
  epicsInt32 sizeX = 0;
  epicsInt32 sizeY = 0;
  epicsInt32 peak_type = 0;
  epicsInt32 intParam = 0.0;
  epicsFloat64 floatParam = 0.0;
  epicsUInt32 minX = 0;
  epicsUInt32 minY = 0;
  epicsUInt32 maxX = 0;
  epicsUInt32 maxY = 0;
  epicsFloat64 result = 0.0;
  epicsFloat64 result_max = 0.0;
  epicsFloat64 scale_factor = 0.0;
  epicsInt32 bg_typex = 0;
  epicsFloat64 bg_c0x = 0.0;
  epicsFloat64 bg_c1x = 0.0;
  epicsFloat64 bg_c2x = 0.0;
  epicsFloat64 bg_c3x = 0.0;
  epicsFloat64 bg_shx = 0.0;
  epicsInt32 bg_typey = 0;
  epicsFloat64 bg_c0y = 0.0;
  epicsFloat64 bg_c1y = 0.0;
  epicsFloat64 bg_c2y = 0.0;
  epicsFloat64 bg_c3y = 0.0;
  epicsFloat64 bg_shy = 0.0;
  epicsInt32 noise_type = 0;
  epicsFloat64 noise_level = 0.0;
  epicsInt32 noise_clamp = 0;
  epicsFloat64 noise_lower = 0.0;
  epicsFloat64 noise_upper = 0.0;
  epicsFloat64 noise = 0.0;
  ADSimPeaksData peak_data;
  ADSimPeaksPeak::e_status peak_status;
  ADSimPeaksPeak::e_type_1d peak_type_1d = m_peaks.e_type_1d::none;
  ADSimPeaksPeak::e_type_2d peak_type_2d = m_peaks.e_type_2d::none;
  
  string functionName(s_className + "::" + __func__);
  
  if (p_NDArray == NULL) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	      "%s invalid NDArray pointer.\n", functionName.c_str());
    return asynError;
  }
  
  p_NDArray->getInfo(&arrayInfo);
  T *pData = static_cast<T*>(p_NDArray->pData);
  size = arrayInfo.nElements;

  getIntegerParam(ADSizeX, &sizeX);
  getIntegerParam(ADSizeY, &sizeY);
  sizeY = std::max(1, sizeY);
  
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
  getIntegerParam(ADSPBGTypeXParam, &bg_typex);
  getDoubleParam(ADSPBGC0XParam, &bg_c0x);
  getDoubleParam(ADSPBGC1XParam, &bg_c1x);
  getDoubleParam(ADSPBGC2XParam, &bg_c2x);
  getDoubleParam(ADSPBGC3XParam, &bg_c3x);
  getDoubleParam(ADSPBGSHXParam, &bg_shx);
  if (m_2d) {
    getIntegerParam(ADSPBGTypeYParam, &bg_typey);
    getDoubleParam(ADSPBGC0YParam, &bg_c0y);
    getDoubleParam(ADSPBGC1YParam, &bg_c1y);
    getDoubleParam(ADSPBGC2YParam, &bg_c2y);
    getDoubleParam(ADSPBGC3YParam, &bg_c3y);
    getDoubleParam(ADSPBGSHYParam, &bg_shy);
  }
  epicsFloat64 bg_x = 0.0;
  epicsFloat64 bg_y = 0.0;
  epicsUInt32 bin_x = 0;
  epicsUInt32 bin_y = 0;
  for (epicsInt32 bin=0; bin<static_cast<epicsInt32>(size); bin++) {
    bin_x = bin % sizeX;
    if (bg_typex == static_cast<epicsUInt32>(e_bg_type::polynomial)) {
      bg_x = bg_c0x + (bin_x-bg_shx)*bg_c1x + pow((bin_x-bg_shx),2)*bg_c2x + pow((bin_x-bg_shx),3)*bg_c3x;
    } else if (bg_typex == static_cast<epicsUInt32>(e_bg_type::exponential)) {
      bg_x = bg_c0x + bg_c1x*(pow(exp(1.0), (bin_x-bg_shx)*bg_c2x));
    }
    if (m_2d) {
      bin_y = floor(bin/sizeX);
      if (bg_typey == static_cast<epicsUInt32>(e_bg_type::polynomial)) {
	bg_y = bg_c0y + (bin_y-bg_shy)*bg_c1y + pow((bin_y-bg_shy),2)*bg_c2y + pow((bin_y-bg_shy),3)*bg_c3y;
      } else if (bg_typey == static_cast<epicsUInt32>(e_bg_type::exponential)) {
	bg_y  = bg_c0y + bg_c1y*(pow(exp(1.0), (bin_y-bg_shy)*bg_c2y));
      }
    }
    pData[bin] += bg_x + bg_y;
  }
  
  //Calculate the peak profile and scale it to the desired height
  for (epicsUInt32 peak=0; peak<m_maxPeaks; peak++) {

    bool no_peak = false;
    if (!m_2d) {
      getIntegerParam(peak, ADSPPeakType1DParam, &peak_type);
      peak_type_1d = static_cast<ADSimPeaksPeak::e_type_1d>(peak_type);
      if (peak_type_1d == m_peaks.e_type_1d::none) {
	no_peak = true;
      }
    } else {
      getIntegerParam(peak, ADSPPeakType2DParam, &peak_type);
      peak_type_2d = static_cast<ADSimPeaksPeak::e_type_2d>(peak_type);
      if (peak_type_2d == m_peaks.e_type_2d::none) {
	no_peak = true;
      }
    }

    if (!no_peak) {
    
      // Get the peak parameters and initialize our peak data object
      peak_data.clear();
      getDoubleParam(peak, ADSPPeakPosXParam, &floatParam);
      peak_data.setPositionX(floatParam);
      getDoubleParam(peak, ADSPPeakPosYParam, &floatParam);
      peak_data.setPositionY(floatParam);
      getDoubleParam(peak, ADSPPeakFWHMXParam, &floatParam);
      peak_data.setFWHMX(floatParam);    
      getDoubleParam(peak, ADSPPeakFWHMYParam, &floatParam);
      peak_data.setFWHMY(floatParam);    
      getDoubleParam(peak, ADSPPeakAmpParam, &floatParam);
      peak_data.setAmplitude(floatParam);
      getDoubleParam(peak, ADSPPeakCorParam, &floatParam);
      peak_data.setCorrelation(floatParam);
      getDoubleParam(peak, ADSPPeakP1Param, &floatParam);
      peak_data.setParam1(floatParam);
      getDoubleParam(peak, ADSPPeakP2Param, &floatParam);
      peak_data.setParam2(floatParam);

      // Read the peak min and max boundaries (and convert to unsigned ints)
      getIntegerParam(ADSPPeakMinXParam, &intParam);
      minX = static_cast<epicsUInt32>(intParam);
      getIntegerParam(ADSPPeakMinYParam, &intParam);
      minY = static_cast<epicsUInt32>(intParam);
      getIntegerParam(ADSPPeakMaxXParam, &intParam);
      maxX = static_cast<epicsUInt32>(intParam);
      getIntegerParam(ADSPPeakMaxYParam, &intParam);
      maxY = static_cast<epicsUInt32>(intParam);
      if (maxX == 0) {
	maxX = sizeX;
      }
      if (maxY == 0) {
	maxY = sizeY;
      }
      
      if (!m_2d) {
	// Compute 1D peak data
	peak_data.setBinX(peak_data.getPositionX());	
	peak_status = m_peaks.compute1D(peak_data, peak_type_1d, result_max);
	if (peak_status == m_peaks.e_status::success) {
	  scale_factor = peak_data.getAmplitude() / zeroCheck(result_max);
	}
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  if ((bin >= minX) && (bin <= maxX)) {
	    peak_data.setBinX(bin);
	    peak_status = m_peaks.compute1D(peak_data, peak_type_1d, result);
	    if (peak_status == m_peaks.e_status::success) {
	      result = (result*scale_factor);
	      pData[bin] += static_cast<T>(result);
	    }
	  }
	}
      } else {
	// Compute 2D peak data
	peak_data.setBinX(peak_data.getPositionX());
	peak_data.setBinY(peak_data.getPositionY());
	peak_status = m_peaks.compute2D(peak_data, peak_type_2d, result_max);
	if (peak_status == m_peaks.e_status::success) {
	  scale_factor = peak_data.getAmplitude() / zeroCheck(result_max);
	}
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  if ((bin_x >= minX) && (bin_x <= maxX) && (bin_y >= minY) && (bin_y <= maxY)) {
	    peak_data.setBinX(bin_x);
	    peak_data.setBinY(bin_y);
	    peak_status = m_peaks.compute2D(peak_data, peak_type_2d, result);
	    if (peak_status == m_peaks.e_status::success) {
	      result = (result*scale_factor);
	      pData[bin] += static_cast<T>(result);
	    }
	  }
	} 
      } // end of if (!m_2d)
      
    } // end of if (!no_peak)
    
  } // end of peak loop
	  
  //Generate noise
  getIntegerParam(ADSPNoiseTypeParam, &noise_type);
  getDoubleParam(ADSPNoiseLevelParam, &noise_level);
  getIntegerParam(ADSPNoiseClampParam, &noise_clamp);
  getDoubleParam(ADSPNoiseLowerParam, &noise_lower);
  getDoubleParam(ADSPNoiseUpperParam, &noise_upper);
  if (noise_type == static_cast<epicsUInt32>(e_noise_type::uniform)) {
    std::uniform_real_distribution<double> dist(-1.0,1.0);
    for (epicsUInt32 bin=0; bin<size; bin++) {
      noise = dist(m_rand_gen);
      noise = noise_level * noise;
      if (noise_clamp != 0) {
	noise = std::max(noise_lower, std::min(noise_upper, noise));
      }
      pData[bin] += static_cast<T>(noise);
    }
  } else if (noise_type == static_cast<epicsUInt32>(e_noise_type::gaussian)) {
    std::normal_distribution<double> dist(0.0,1.0);
    for (epicsUInt32 bin=0; bin<size; bin++) {
      noise = dist(m_rand_gen);
      noise = noise_level * noise;
      if (noise_clamp != 0) {
	noise = std::max(noise_lower, std::min(noise_upper, noise));
      } 
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
  static const iocshArg ADSimPeaksConfigArg1 = {"Max Size X", iocshArgInt};
  static const iocshArg ADSimPeaksConfigArg2 = {"Max Size Y", iocshArgInt};
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

