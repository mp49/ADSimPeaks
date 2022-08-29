/**
 * \brief areaDetector driver to simulate 1D and 2D peaks with background 
 *        profiles and noise. 
 *
 * This areaDetector driver can be used to simulate semi-realistic looking
 * data in 1D and 2D. It can produce a 1D or 2D NDArray object of variable size and 
 * of different data types. The data can contain a background and any number 
 * of peaks of a few different shapes, with the option to add different kinds of 
 * noise to the signal.
 *
 * Supported 1D peak shapes are:
 * 1) Square
 * 2) Triangle
 * 3) Gaussian (normal
 * 4) Lorentzian (also known as Cauchy)
 * 5) Voigt (implemented as a psudo-Voigt, which is an approximation)
 * 6) Laplace
 *
 * Supported 2D peak shapes are:
 * 1) Square
 * 2) Pyramid
 * 3) Eliptical Cone
 * 4) Gaussian (normal)
 * 5) Lorentzian (also known as Cauchy)
 * 6) Voigt (implemented as a psudo-Voigt, which is an approximation)
 * 7) Laplace
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

// Static Data (including some precalculated data for the function distributions)
// Class name
const string ADSimPeaks::s_className = "ADSimPeaks";
// Constant used to test for 0.0
const epicsFloat64 ADSimPeaks::s_zeroCheck = 1e-12;
// Constant 2.0*sqrt(2.0*log(2.0))
const epicsFloat64 ADSimPeaks::s_2s2l2 = 2.3548200450309493;
// Constant sqrt(2.0*M_PI)
const epicsFloat64 ADSimPeaks::s_s2pi = 2.5066282746310002;
// Constat 2.0*log(2.0)
const epicsFloat64 ADSimPeaks::s_2l2 = 1.3862943611198906;
// Constant data for the psudoVoigt eta parameter
const epicsFloat64 ADSimPeaks::s_pv_p1 = 2.69269;
const epicsFloat64 ADSimPeaks::s_pv_p2 = 2.42843;
const epicsFloat64 ADSimPeaks::s_pv_p3 = 4.47163;
const epicsFloat64 ADSimPeaks::s_pv_p4 = 0.07842;
const epicsFloat64 ADSimPeaks::s_pv_e1 = 1.36603;
const epicsFloat64 ADSimPeaks::s_pv_e2 = 0.47719;
const epicsFloat64 ADSimPeaks::s_pv_e3 = 0.11116;

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
  createParam(ADSPBGC0XParamString, asynParamFloat64, &ADSPBGC0XParam);
  createParam(ADSPBGC1XParamString, asynParamFloat64, &ADSPBGC1XParam);
  createParam(ADSPBGC2XParamString, asynParamFloat64, &ADSPBGC2XParam);
  createParam(ADSPBGC3XParamString, asynParamFloat64, &ADSPBGC3XParam);
  createParam(ADSPBGSHXParamString, asynParamFloat64, &ADSPBGSHXParam);
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
    callParamCallbacks(peak);
  }
  //Background Params X
  paramStatus = ((setDoubleParam(ADSPBGC0XParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC1XParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC2XParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGC3XParam, 0.0) == asynSuccess) && paramStatus);
  paramStatus = ((setDoubleParam(ADSPBGSHXParam, 0.0) == asynSuccess) && paramStatus);
  //Background Params Y
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
	if (intParam == static_cast<epicsUInt32>(e_peak_type_1d::none)) {
	  fprintf(fp, "   none (disabled)\n");
	}
      } else {
	getIntegerParam(i, ADSPPeakType2DParam, &intParam);
	if (intParam == static_cast<epicsUInt32>(e_peak_type_2d::none)) {
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
  epicsInt32 sizeX = 0;
  epicsInt32 sizeY = 0;
  epicsInt32 peak_type = 0;
  epicsFloat64 peak_pos_x = 0.0;
  epicsFloat64 peak_fwhm_x = 0.0;
  epicsFloat64 peak_pos_y = 0.0;
  epicsFloat64 peak_fwhm_y = 0.0;
  epicsFloat64 peak_amp = 0.0;
  epicsFloat64 peak_cor = 0.0;
  epicsFloat64 peak_p1 = 0.0;
  epicsFloat64 peak_p2 = 0.0;
  epicsFloat64 result = 0.0;
  epicsFloat64 result_max = 0.0;
  epicsFloat64 scale_factor = 0.0;
  epicsFloat64 bg_c0x = 0.0;
  epicsFloat64 bg_c1x = 0.0;
  epicsFloat64 bg_c2x = 0.0;
  epicsFloat64 bg_c3x = 0.0;
  epicsFloat64 bg_shx = 0.0;
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
  getDoubleParam(ADSPBGC0XParam, &bg_c0x);
  getDoubleParam(ADSPBGC1XParam, &bg_c1x);
  getDoubleParam(ADSPBGC2XParam, &bg_c2x);
  getDoubleParam(ADSPBGC3XParam, &bg_c3x);
  getDoubleParam(ADSPBGSHXParam, &bg_shx);
  if (m_2d) {
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
    bg_x = bg_c0x + (bin_x-bg_shx)*bg_c1x + pow((bin_x-bg_shx),2)*bg_c2x + pow((bin_x-bg_shx),3)*bg_c3x;
    if (m_2d) {
      bin_y = floor(bin/sizeX);
      bg_y = bg_c0y + (bin_y-bg_shy)*bg_c1y + pow((bin_y-bg_shy),2)*bg_c2y + pow((bin_y-bg_shy),3)*bg_c3y;
    }
    pData[bin] += bg_x + bg_y;
  }
  
  //Calculate the peak profile and scale it to the desired height
  for (epicsUInt32 peak=0; peak<m_maxPeaks; peak++) {
    
    getDoubleParam(peak, ADSPPeakPosXParam, &peak_pos_x);
    getDoubleParam(peak, ADSPPeakFWHMXParam, &peak_fwhm_x);
    getDoubleParam(peak, ADSPPeakAmpParam, &peak_amp);
    getDoubleParam(peak, ADSPPeakP1Param, &peak_p1);
    getDoubleParam(peak, ADSPPeakP2Param, &peak_p2);
    if (!m_2d) {
      getIntegerParam(peak, ADSPPeakType1DParam, &peak_type);
    } else {
      getIntegerParam(peak, ADSPPeakType2DParam, &peak_type);
      getDoubleParam(peak, ADSPPeakPosYParam, &peak_pos_y);
      getDoubleParam(peak, ADSPPeakFWHMYParam, &peak_fwhm_y);
      getDoubleParam(peak, ADSPPeakCorParam, &peak_cor);
    }

    if (!m_2d) {
      // Compute 1D peaks
      if (peak_type == static_cast<epicsUInt32>(e_peak_type_1d::gaussian)) {
	computeGaussian(peak_pos_x, peak_fwhm_x, peak_pos_x, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  computeGaussian(peak_pos_x, peak_fwhm_x, bin, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_1d::lorentz)) {
	computeLorentz(peak_pos_x, peak_fwhm_x, peak_pos_x, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  computeLorentz(peak_pos_x, peak_fwhm_x, bin, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_1d::pseudovoigt)) {
	computePseudoVoigt(peak_pos_x, peak_fwhm_x, peak_pos_x, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  computePseudoVoigt(peak_pos_x, peak_fwhm_x, bin, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_1d::laplace)) {
	computeLaplace(peak_pos_x, peak_fwhm_x, peak_pos_x, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  computeLaplace(peak_pos_x, peak_fwhm_x, bin, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_1d::triangle)) {
	computeTriangle(peak_pos_x, peak_fwhm_x, peak_pos_x, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  computeTriangle(peak_pos_x, peak_fwhm_x, bin, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_1d::square)) {
	computeSquare(peak_pos_x, peak_fwhm_x, peak_pos_x, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  computeSquare(peak_pos_x, peak_fwhm_x, bin, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_1d::moffat)) {
	// Use P1 as the 'beta' parameter
	computeMoffat(peak_pos_x, peak_fwhm_x, peak_p1, peak_pos_x, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  computeMoffat(peak_pos_x, peak_fwhm_x, peak_p1, bin, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      }
    } else {
      // Compute 2D peaks
      if (peak_type == static_cast<epicsUInt32>(e_peak_type_2d::gaussian)) {
	computeGaussian2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, peak_pos_x, peak_pos_y, peak_cor, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  computeGaussian2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, bin_x, bin_y, peak_cor, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_2d::lorentz)) {
	// Use the X FWHM as the overall FWHM
	computeLorentz2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_pos_x, peak_pos_y, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  computeLorentz2D(peak_pos_x, peak_pos_y, peak_fwhm_x, bin_x, bin_y, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_2d::pseudovoigt)) {
	computePseudoVoigt2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, peak_pos_x, peak_pos_y, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  computePseudoVoigt2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, bin_x, bin_y, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_2d::laplace)) {
	computeLaplace2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, peak_pos_x, peak_pos_y, peak_cor, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  computeLaplace2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, bin_x, bin_y, peak_cor, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_2d::pyramid)) {
	computePyramid2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, peak_pos_x, peak_pos_y, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  computePyramid2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, bin_x, bin_y, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_2d::cone)) {
	computeCone2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, peak_pos_x, peak_pos_y, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  computeCone2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, bin_x, bin_y, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_2d::square)) {
	computeSquare2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, peak_pos_x, peak_pos_y, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  computeSquare2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_fwhm_y, bin_x, bin_y, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      } else if (peak_type == static_cast<epicsUInt32>(e_peak_type_2d::moffat)) {
	// Use P1 as the 'beta' parameter, and use the X FWHM as the overall FWHM
	computeMoffat2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_p1, peak_pos_x, peak_pos_y, &result_max);
	scale_factor = peak_amp / zeroCheck(result_max);
	for (epicsUInt32 bin=0; bin<size; bin++) {
	  bin_x = bin % sizeX;
	  bin_y = floor(bin/sizeX);
	  computeMoffat2D(peak_pos_x, peak_pos_y, peak_fwhm_x, peak_p1, bin_x, bin_y, &result);
	  result = (result*scale_factor);
	  pData[bin] += static_cast<T>(result);
	}
      }
    } // end of if (!m_2d)
    
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
  fwhm = std::max(1.0, fwhm);

  // This uses some class static constant data that has been pre-computed
  epicsFloat64 sigma = fwhm / s_2s2l2;
  
  *result = (1.0 / (sigma*s_s2pi)) * exp(-(((bin-pos)*(bin-pos))) / (2.0*(sigma*sigma)));

  return asynSuccess;
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
  fwhm = std::max(1.0, fwhm);
  
  epicsFloat64 gamma = fwhm / 2.0;
  *result = (1 / (M_PI*gamma)) * ((gamma*gamma) / (((bin-pos)*(bin-pos)) + (gamma*gamma)));

  return asynSuccess;
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
  epicsFloat64 fwhm_g = 0.0;
  epicsFloat64 fwhm_l = 0.0;
  epicsFloat64 eta = 0.0;
  epicsFloat64 gaussian = 0.0;
  epicsFloat64 lorentz = 0.0;

  fwhm = std::max(1.0, fwhm);
  
  //This implementation assumes the FWHM of the Gaussian and Lorentz is the same. However, we
  //still use the full approximation for the Pseudo-Voigt total FWHM (fwhm_tot) and use two FWHM parameters
  //(fwhm_g and fwhm_l), so that this function can easily be modified to use a different Gaussian
  //and Lorentzian FWHM.
  
  fwhm_g = fwhm;
  fwhm_l = fwhm;

  computePseudoVoigtEta(fwhm_g, fwhm_l, &eta);
  computeGaussian(pos, fwhm, bin, &gaussian);
  computeLorentz(pos, fwhm, bin, &lorentz);

  *result = ((1.0 - eta)*gaussian) + (eta*lorentz);

  return asynSuccess;
}

/**
 * Implementation of a Laplace function which has center 'pos' and full width half max 'fwhm'.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Laplace_distribution
 *
 * The FWHM can be calculated by determining the height when pos=bin, then taking 1/2 
 * that value and determining the value of 'bin' when the function equals that height, 
 * then doubling the result. Then we can calculate 'b' from our input FWHM.
 *
 * /arg /c pos The center of the distribution
 * /arg /c fwhm The FWHM of the distribution
 * /arg /c bin The position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeLaplace(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result)
{
  fwhm = std::max(1.0, fwhm);

  // This uses some class static constant data that has been pre-computed
  epicsFloat64 b = fwhm / s_2l2;
  *result = (1.0/(2.0*b)) * exp(-((abs(bin - pos))/b));

  return asynSuccess;
}


/**
 * Implementation of a simple isosceles triangle which has center 'pos' and full width 
 * half max 'fwhm'.
 *
 * /arg /c pos The center of the distribution
 * /arg /c fwhm The FWHM of the distribution
 * /arg /c bin The position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeTriangle(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result)
{
  fwhm = std::max(1.0, fwhm);

  epicsFloat64 peak = 1.0;
  epicsFloat64 b = peak/fwhm;

  if (bin <= static_cast<epicsInt32>(pos)) {
    b = b*1.0;
  } else {
    b = b*-1.0;
  }

  *result = peak + b*(bin-pos);
  *result = std::max(0.0, *result);

  return asynSuccess;
}

/**
 * Implementation of a simple square which has center 'pos' and full width 
 * half max 'fwhm'.
 *
 * /arg /c pos The center of the distribution
 * /arg /c fwhm The FWHM of the distribution
 * /arg /c bin The position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeSquare(epicsFloat64 pos, epicsFloat64 fwhm, epicsInt32 bin, epicsFloat64 *result)
{
  fwhm = std::max(1.0, fwhm);

  epicsFloat64 peak = 1.0;
  
  if ((bin > static_cast<epicsInt32>(pos - fwhm/2.0)) && (bin <= static_cast<epicsInt32>(pos + fwhm/2.0))) {
    *result = peak;
  } else {
    *result = 0.0;
  }

  return asynSuccess;
}

/**
 * Implementation of a Moffat distribution which has center 'pos' and full width 
 * half max 'fwhm'. The Moffat function is determined by the alpha and beta 'seeing'
 * parameters. We calculate alpha based on the input FWHM and beta. The beta parameter
 * determins the shape of the function.
 *
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Moffat_distribution
 *
 * alpha = FWHM / (2 * sqrt(2^(1/beta) - 1))
 *
 * /arg /c pos The center of the distribution
 * /arg /c fwhm The FWHM of the distribution
 * /arg /c beta The beta seeing parameter
 * /arg /c bin The position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeMoffat(epicsFloat64 pos, epicsFloat64 fwhm, epicsFloat64 beta,
				     epicsInt32 bin, epicsFloat64 *result)
{
  fwhm = std::max(1.0, fwhm);
  beta = zeroCheck(beta);
  epicsFloat64 peak = 1.0;
  epicsFloat64 alpha = fwhm / (2.0 * sqrt(pow(2.0,1.0/beta) - 1));
  epicsFloat64 alpha2 = alpha*alpha;

  *result = peak * pow((1 + (((bin-pos)*(bin-pos))/alpha2)),-beta);

  return asynSuccess;
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
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);
  rho = std::min(1.0, std::max(-1.0, rho));

  // This uses some class static constant data that has been pre-computed
  epicsFloat64 x_sig = x_fwhm / s_2s2l2;
  epicsFloat64 y_sig = y_fwhm / s_2s2l2;

  epicsFloat64 xy_amp = 1.0 / (2.0 * M_PI * x_sig * y_sig * sqrt(1-(rho*rho)));
  epicsFloat64 xy_factor = -1 / (2*(1-(rho*rho)));
  epicsFloat64 xy_calc1 = (x_bin-x_pos)/x_sig;
  epicsFloat64 xy_calc2 = (y_bin-y_pos)/y_sig;
    
  *result = xy_amp * exp(xy_factor*(xy_calc1*xy_calc1 - 2*rho*xy_calc1*xy_calc2 + xy_calc2*xy_calc2));

  return asynSuccess;
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
  fwhm = std::max(1.0, fwhm);
  
  epicsFloat64 gamma = fwhm / 2.0;
  epicsFloat64 xy_calc1 = x_bin-x_pos;
  epicsFloat64 xy_calc2 = y_bin-y_pos;
  
  *result = (1 / (2*M_PI)) * (gamma / pow(((xy_calc1*xy_calc1) + (xy_calc2*xy_calc2) + gamma*gamma),1.5));

  return asynSuccess;
}

/**
 * Implementation of the approximation of the bivariate Voigt function 
 * (known as the Psudo-Voigt) which has center (x,y) and 
 * full width half max 'fwhm'. The Guassian part of the function 
 * can be defined with different FWHM parameters in X and Y, and with
 * a skewed shape, but for the purposes of this approximation we
 * assume it has zero skew and an average is taken 
 * as the Lorenztian FWHM component.
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Voigt_profile
 *
 * /arg /c x_pos The X coordinate of the distribution
 * /arg /c y_pos The Y coordinate of the distribution
 * /arg /c x_fwhm The X dimension FWHM of the distribution
 * /arg /c y_fwhm The Y dimension FWHM of the distribution
 * /arg /c x_bin The X position to use for the function
 * /arg /c y_bin The Y position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computePseudoVoigt2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
					    epicsFloat64 x_fwhm, epicsFloat64 y_fwhm,
					    epicsInt32 x_bin, epicsInt32 y_bin,
					    epicsFloat64 *result)
{
  epicsFloat64 fwhm_av = 0.0;
  epicsFloat64 fwhm_g = 0.0;
  epicsFloat64 fwhm_l = 0.0;
  epicsFloat64 eta = 0.0;
  epicsFloat64 gaussian = 0.0;
  epicsFloat64 lorentz = 0.0;
  
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);
  
  fwhm_av = (x_fwhm+y_fwhm)/2.0;
  fwhm_g = fwhm_av;
  fwhm_l = fwhm_av;

  computePseudoVoigtEta(fwhm_g, fwhm_l, &eta);
  computeGaussian2D(x_pos, y_pos, x_fwhm, y_fwhm, x_bin, y_bin, 0.0, &gaussian);
  computeLorentz2D(x_pos, y_pos, fwhm_av, x_bin, y_bin, &lorentz);

  *result = ((1.0 - eta)*gaussian) + (eta*lorentz);

  return asynSuccess;
}

asynStatus ADSimPeaks::computePseudoVoigtEta(epicsFloat64 fwhm_g, epicsFloat64 fwhm_l, epicsFloat64 *eta)
{
  epicsFloat64 fwhm_sum = 0.0;
  epicsFloat64 fwhm_tot = 0.0;

  // This uses some class static constant data that has been pre-computed
  fwhm_sum = pow(fwhm_g,5) + (s_pv_p1*pow(fwhm_g,4)*fwhm_l) + (s_pv_p2*pow(fwhm_g,3)*pow(fwhm_l,2)) +
            (s_pv_p3*pow(fwhm_g,2)*pow(fwhm_l,3)) + (s_pv_p4*fwhm_g*pow(fwhm_l,4)) + pow(fwhm_l,5);
  fwhm_tot = pow(fwhm_sum,0.2);
  
  *eta = ((s_pv_e1*(fwhm_l/fwhm_tot)) - (s_pv_e2*pow((fwhm_l/fwhm_tot),2)) + (s_pv_e3*pow((fwhm_l/fwhm_tot),3)));
  
  return asynSuccess;
}

/**
 * Implementation of a bivariate Laplace function which has center (x,y) and full width half max 
 * 'x_fwhm' and 'y_fwhm', with X and Y correlation rho (where -1<=rho<=1).
 * 
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Multivariate_Laplace_distribution
 *
 * We calculate the 'b' scale factor in the same way as for ADSimPeaks::computeLaplace,
 * then we calculate the standard deviation. The actual bivariate Laplace uses a modified
 * Bessle function of the second kind, see:
 * https://en.wikipedia.org/wiki/Bessel_function#Modified_Bessel_functions:_I%CE%B1,_K%CE%B1
 * but to avoid having to calculate this we just assume a decaying expoential, which seems 
 * like a good approximation. 
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
asynStatus ADSimPeaks::computeLaplace2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
					epicsFloat64 x_fwhm, epicsFloat64 y_fwhm,
					epicsInt32 x_bin, epicsInt32 y_bin,
					epicsFloat64 rho, epicsFloat64 *result)
{
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);
  rho = std::min(1.0, std::max(-1.0, rho));

  // This uses some class static constant data that has been pre-computed
  // Standard deviation is sqrt(2) * the scale factor b
  epicsFloat64 x_sig = sqrt(2.0) * (x_fwhm / s_2l2);
  epicsFloat64 y_sig = sqrt(2.0) * (y_fwhm / s_2l2);

  epicsFloat64 xy_amp = 1.0 / (M_PI * x_sig * y_sig * sqrt(1-(rho*rho)));
  epicsFloat64 xy_calc1 = (x_bin-x_pos)/x_sig;
  epicsFloat64 xy_calc2 = (y_bin-y_pos)/y_sig;
    
  *result = xy_amp * exp(-sqrt((2.0*(xy_calc1*xy_calc1 - 2*rho*xy_calc1*xy_calc2 + xy_calc2*xy_calc2))/(1-(rho*rho))));

  return asynSuccess;
}

/**
 * Implementation of a simple pyramid which has center (x_pos,y_pos) and full width 
 * half max 'x_fwhm' and 'y_fwhm'.
 *
 * /arg /c x_pos The X coordinate of the distribution
 * /arg /c y_pos The Y coordinate of the distribution
 * /arg /c x_fwhm The X dimension FWHM of the distribution
 * /arg /c y_fwhm The Y dimension FWHM of the distribution
 * /arg /c x_bin The X position to use for the function
 * /arg /c y_bin The Y position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computePyramid2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
				     epicsFloat64 x_fwhm, epicsFloat64 y_fwhm,
				     epicsInt32 x_bin, epicsInt32 y_bin,
				     epicsFloat64 *result)
{
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);

  epicsFloat64 peak = 1.0;
  epicsFloat64 b = peak/x_fwhm;
  epicsFloat64 c = peak/y_fwhm;
  if ((x_bin <= static_cast<epicsInt32>(x_pos)) && (y_bin <= static_cast<epicsInt32>(y_pos))) {
    b = b*1.0;
    c = c*1.0;
  } else if ((x_bin <= static_cast<epicsInt32>(x_pos)) && (y_bin > static_cast<epicsInt32>(y_pos))) {
    b = b*1.0;
    c = c*-1.0;
  } else if ((x_bin > static_cast<epicsInt32>(x_pos)) && (y_bin <= static_cast<epicsInt32>(y_pos))) {
    b = b*-1.0;
    c = c*1.0;
  } else {
    b = b*-1.0;
    c = c*-1.0;
  }
  
  *result = peak + b*(x_bin-x_pos) + c*(y_bin-y_pos);
  *result = std::max(0.0, *result);
  
  return asynSuccess;
}

/**
 * Implementation of an eliptical cone which has center (x_pos,y_pos) and full width 
 * half max 'x_fwhm' and 'y_fwhm'.
 *
 * /arg /c x_pos The X coordinate of the distribution
 * /arg /c y_pos The Y coordinate of the distribution
 * /arg /c x_fwhm The X dimension FWHM of the distribution
 * /arg /c y_fwhm The Y dimension FWHM of the distribution
 * /arg /c x_bin The X position to use for the function
 * /arg /c y_bin The Y position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeCone2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
				     epicsFloat64 x_fwhm, epicsFloat64 y_fwhm,
				     epicsInt32 x_bin, epicsInt32 y_bin,
				     epicsFloat64 *result)
{
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);

  epicsFloat64 peak = x_fwhm + y_fwhm;

  epicsFloat64 height = 0.0;
  // Find the distance of this point from the center of the ellipse
  epicsFloat64 d = sqrt((x_bin-x_pos)*(x_bin-x_pos) + (y_bin-y_pos)*(y_bin-y_pos));
  if (d != 0) {
    // Find the angle of this point
    epicsFloat64 theta = asin((y_bin-y_pos)/d);
    // Find the radius of the ellipse defining the edge of the cone at this same angle 
    epicsFloat64 r = (x_fwhm * y_fwhm) / sqrt(pow(y_fwhm*cos(theta),2) + pow(x_fwhm*sin(theta),2));
    // Find the height of the cone inside the ellipse
    height = (r-d) * (peak/r);
  } else {
    height = peak;
  }
  
  *result = height;
  *result = std::max(0.0, *result);
  
  return asynSuccess;
}

/**
 * Implementation of a cube peak, which looks like a square from the top,
 *  which has center (x_pos,y_pos) and full width half max 'x_fwhm' and 'y_fwhm'.
 *
 * /arg /c x_pos The X coordinate of the distribution
 * /arg /c y_pos The Y coordinate of the distribution
 * /arg /c x_fwhm The X dimension FWHM of the distribution
 * /arg /c y_fwhm The Y dimension FWHM of the distribution
 * /arg /c x_bin The X position to use for the function
 * /arg /c y_bin The Y position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeSquare2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
				       epicsFloat64 x_fwhm, epicsFloat64 y_fwhm,
				       epicsInt32 x_bin, epicsInt32 y_bin,
				       epicsFloat64 *result)
{
  x_fwhm = std::max(1.0, x_fwhm);
  y_fwhm = std::max(1.0, y_fwhm);

  epicsFloat64 peak = 1.0;

  if ((x_bin >  static_cast<epicsInt32>(x_pos - x_fwhm/2.0)) &&
      (x_bin <= static_cast<epicsInt32>(x_pos + x_fwhm/2.0)) &&
      (y_bin >  static_cast<epicsInt32>(y_pos - y_fwhm/2.0)) &&
      (y_bin <= static_cast<epicsInt32>(y_pos + y_fwhm/2.0))) {
    *result = peak;
  } else {
    *result = 0.0;
  }
  
  return asynSuccess;
}

/**
 * Implementation of a Moffat distribution which has center (x_pos,y_pos) and full width 
 * half max 'fwhm'. The Moffat function is determined by the alpha and beta 'seeing'
 * parameters. We calculate alpha based on the input FWHM and beta. The beta parameter
 * determins the shape of the function. Large values of beta (>>1) will cause the distribution
 * to be similar to a gaussian, and small values (<1) will cause it to look like an exponential.
 *
 * For more information on this see:
 * https://en.wikipedia.org/wiki/Moffat_distribution
 *
 * alpha = FWHM / (2 * sqrt(2^(1/beta) - 1))

 * /arg /c x_pos The X coordinate of the distribution
 * /arg /c y_pos The Y coordinate of the distribution
 * /arg /c fwhm The FWHM of the distribution (in X and Y)
 * /arg /c beta The beta seeing parameter
 * /arg /c x_bin The X position to use for the function
 * /arg /c y_bin The Y position to use for the function
 * /arg /c result Pointer which will be used to return the result of the calculation
 *
 * /return asynStatus
 */
asynStatus ADSimPeaks::computeMoffat2D(epicsFloat64 x_pos, epicsFloat64 y_pos,
				       epicsFloat64 fwhm, epicsFloat64 beta,
				       epicsInt32 x_bin, epicsInt32 y_bin,
				       epicsFloat64 *result)
{
  fwhm = std::max(1.0, fwhm);
  beta = zeroCheck(beta);
  epicsFloat64 peak = 1.0;
  epicsFloat64 alpha = fwhm / (2.0 * sqrt(pow(2.0,1.0/beta) - 1));
  epicsFloat64 alpha2 = alpha*alpha;

  *result = peak * pow((1 + ((((x_bin-x_pos)*(x_bin-x_pos)) + ((y_bin-y_pos)*(y_bin-y_pos)))/alpha2)),-beta);
  
  return asynSuccess;
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

