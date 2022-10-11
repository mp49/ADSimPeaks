# ADSimPeaks

EPICS areaDetector driver to simulate detector peaks with background 
and noise in 1D or 2D.

## Description

This areaDetector driver can be used to simulate semi-realistic diffraction
data in 1D and 2D. It can produce a 1D or 2D NDArray object of variable size and 
of different data types. The data can contain a background profile and any number 
of peaks of a few different shapes, with the option to add different kinds of 
noise to the signal.

The background type can either be a 3rd order polynomial, so that the shape can be 
a flat offset, a slope or a curve, or an exponential with a slope and offset. 

The noise type can be either uniformly distributed or distributed
according to a Gaussian profile. 

The width of the peaks can be restricted by setting hard lower and upper
boundaries, which may be useful in some cases (such as saving CPU). 

## Getting Started

There are two example IOC applications packaged with this module:

* example - 1D ADSimPeaks example
* example2d - 2D ADSimPeaks example

The IOC applications demonstrate how to instantiate the driver for 1D data or 2D data. 

For 1D data the driver is instantiated in the IOC startup script like:
```
# Arguments:
# 1 - Asyn port name
# 2 - Maximum size of the NDArray X dimension
# 3 - Maximum size of the NDArray Y dimension (set to 0 for 1D data)
# 4 - Maximum number of peaks (which defines the maximum number of Asyn addresses)
# 5 - Starting data type for the NDArray object (NDDataType_t)
# 6 - Maximum buffers (0 = default)
# 7 - 
#  
ADSimPeaksConfig(D1.SIM,65536,0,10,3,0,0,0,0)
```

## Usage

## Developer

The build has been tested on Red Hat Enterprise Linux 7 and 8 with:

* EPICS base 7.0.6.1
* Asyn R4-43
* areaDetector ADCore R3-11

The project requires a reasonably modern C++ compiler (C++11 or newer). 

List of main classes (header files and source files):

ADSimPeaks - the main areaDetector (inherits from ADBase)   
ADSimPeaksPeak - contains the implementation of the various peak shapes  
ADSimPeaksData - container class to hold peak information  

## License

Copyright 2021, UT-Battelle, LLC  
All rights reserved  
OPEN-SOURCE LICENSE  

(see the LICENSE file)

## Userful Links

1. [EPICS](https://epics-controls.org/)
2. [Asyn Module](https://github.com/epics-modules/asyn)
3. [areaDetector Project](https://github.com/areaDetector)

## Contact

For general help contact EPICS Tech-Talk mailing list.

