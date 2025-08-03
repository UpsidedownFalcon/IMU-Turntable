# High-Tech Fidget Spinner: 3-Axis IMU Turntable 

Welcome! This is an open-source project for the construction of a 3-DoF IMU turntable, named to be a "High-Tech Fidget Spinner". It is a low-cost alternative to commercial solutions which can cost in the 5 to 6-figure USD range, to be used for the validation and characterisation of IMU sensors. 

<img width="1226" height="529" alt="System Architecture" src="https://github.com/user-attachments/assets/548bdc92-e83a-4a56-b5be-b41e3e2b28fd" />

The repository contains supporting files to the conference paper titled "High-Tech Fidget Spinner: Open-Source 3-Axis Turntable" from UbiComp Companion ’25, October 12–16, 2025, Espoo, Finland: https://doi.org/10.1145/3714394.3756256 

The top-level strucutre of the repository is as follows: 

```
.
├── build              # 3D-printing files, instructions and BoM to construct the hardware    
├── docs               # Project development and conference paper documentation 
├── src                # Code for the webapp and controller firmware    
├── testing            # Files from testing the High-Tech Fidget Spinner 
├── tools              # Files to select approrpriate stepper motors 
├── LICENSE.md         
└── README.md           
```

Each directory has its own README file that contains further details about it. 

## Getting started 

0) Familiarise yoursel with the directories in this repository and their README files and the overview paper at https://doi.org/10.1145/3714394.3756256  

1) Construct the hardware as described in [```/build/```](/build/) 

2) Upload the firmware as described in [```/src/```](/src/)

3) Launch the webapp as described in [```/src/```](/src/) 

4) You're ready to use the project! Usage instructions can be found at [```/docs/how-to-use.md```](/docs/how-to-use.md) 
