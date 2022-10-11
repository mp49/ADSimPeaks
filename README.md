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

The two following screenshots are an example of the types of plots that can be created by this driver. 

![1D Spectrum with several peaks, polynomial background and noise](./docs/images/complex_1d_plot.PNG)
![2D Spectrum with several peaks, constant background and noise](./docs/images/complex_2d_plot.PNG)

## Getting Started

There are two example IOC applications packaged with this module:

* example - 1D ADSimPeaks example
* example2d - 2D ADSimPeaks example

### IOC startup script

The IOC applications demonstrate how to instantiate the driver. The same function is used for both 1D and 2D data, with a 2D driver being setup if the Y dimension is non-zero. 

For 1D data (max size = 65536) the driver is instantiated in the IOC startup script like:
```
# Arguments:
# 1 - Asyn port name
# 2 - Maximum size of the NDArray X dimension
# 3 - Maximum size of the NDArray Y dimension (set to 0 for 1D data)
# 4 - Maximum number of peaks (which defines the maximum number of Asyn addresses)
# 5 - Starting data type for the NDArray object (NDDataType_t)
# 6 - Maximum buffers (0 = unlimited)
# 7 - Maximum memory (0 = unlimited)
# 8 - Priority (0 = default)
# 9 - Stack Size (0 = default)
ADSimPeaksConfig(D1.SIM,65536,0,10,3,0,0,0,0)
```

And for 2D data (1024 x 1024) it would be:
```
ADSimPeaksConfig(D2.SIM,1024,1024,10,3,0,0,0,0)
```

In both the above cases the data type is UInt16. The ```NDDataType_t``` enum can be found in the areaDetector documentation, however the driver supports changing the data type at runtime.  

The example IOC applications also use the areaDetector PVAccess plugin to export the data over PVAccess for visualization in a client application. This is done like:
```
NDPvaConfigure(D1.PV1,100,0,D1.SIM,0,"ST99:Det:Det1:PV1:Array",0,0,0)
```
where ```ST99:Det:Det1:PV1:Array``` is the name of the PVAccess channel used to access the NTNDArray object. If you need to use Channel Access, then use the ```StdArrays``` plugin instead.

### Setting up the database

The example IOC applications show that the database can be built using substitution files. For example, the database for the 1D driver can be built using:
```
file ADSimPeaks.template
{
pattern {P, R, PORT, ADDR, TIMEOUT}
        {ST99:Det, :Det1:, D1.SIM, 0, 1}
}

file ADSimPeaks1DBackground.template
{
pattern {P, R, PORT, ADDR, TIMEOUT}
        {ST99:Det, :Det1:, D1.SIM, 0, 1}
}

file ADSimPeaks1DPeak.template
{
pattern {P, R, PORT, ADDR, TIMEOUT, PEAK}
        {ST99:Det, :Det1:, D1.SIM, 0, 1, 0}
        {ST99:Det, :Det1:, D1.SIM, 0, 1, 1}
        {ST99:Det, :Det1:, D1.SIM, 0, 1, 2}
        {ST99:Det, :Det1:, D1.SIM, 0, 1, 3}
        {ST99:Det, :Det1:, D1.SIM, 0, 1, 4}
        {ST99:Det, :Det1:, D1.SIM, 0, 1, 5}
        {ST99:Det, :Det1:, D1.SIM, 0, 1, 6}
        {ST99:Det, :Det1:, D1.SIM, 0, 1, 7}
}
```

There are similar database template files for the 2D case (ADSimPeaks2DBackground.template and ADSimPeaks2DPeak.template). ```ADSimPeaks1DPeak.template``` or ```ADSimPeaks2DPeak.template``` files should be instantiated for each peak that will need to be configured. 

The example database substitution files also demonstrates how to use the database template for the pvaPlugin support. 

There are additional database template files used in the example IOC applications to deal with autosave status and adding busy record support. So these examples also require the use of those modules, which are common EPICS modules (see the [Useful Links](#useful-links) section).


## Usage

## Developer

The build has been tested on Red Hat Enterprise Linux 8 with:

* EPICS base 7.0.6.1
* Asyn R4-43
* areaDetector ADCore R3-11

The bundled example IOC applications require the use of EPICS 7 as they use the QSRV module to export data over PVAccess. However, the ADSimPeaks driver itself should work with EPICS 3.14.12.X and 3.15.X as long as a recent version of Asyn and areaDetector are used.  

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
4. [busy record](https://github.com/epics-modules/busy)
5. [autosave support](https://github.com/epics-modules/autosave)

## Contact

Submit a ticket to the project, or for general help contact the EPICS Tech-Talk mailing list.

