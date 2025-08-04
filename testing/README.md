# ```/testing/```

The High-Tech Fidget Spinner was tested in a motion capture lab; IR reflectors were placed on all rotating frames and motion was captured using an Optitrak Prime X 22 system while each axis underwent a step trajectory individually. The stepped trajectory was a rotation from 0 deg to 90 deg in increments of 10 deg every 2 s - it is saved onto [```/testing/step.bin```](/testing/step.bin). The measured trajectory for the X, Y and Z axes are saved in [```/testing/Test_X.csv```](/testing/Test_X.csv), [```testing/Test_Y.csv```](/testing/Test_Y.csv) and [```/testing/Test_Z.csv```](/testing/Test_Z.csv) respectively. The mean deviation and resolution for each axis was analysed. 

```
├── error-plots.zip          # Error distribution plots 
├── Plot.py                  # Python script for statisical analysis 
├── README.md 
├── step.bin                 # True trajectory profile 
├── Test_X.csv               # Measured X trajectory profile 
├── Test_Y.csv               # Measured Y trajectory profile 
└── Test_Z.csv               # Measured Z trajectory profile 
``` 

## Error Statistics 
Data manually aligned in time and angle axes at the initial position 

### X-Axis 
- Trajectory starts at t = 5.8612s 
- 0 angular offset 
- Systematic error after t = 24.3331s: wires got stuck causing stepper to skip steps 
    => Hence data only analysed in the domain **t = [5.8612, 24.3331]s** to remove systematic error 

=> Deviation = 0.0277 +- 0.3226 deg

=> |Resolution| = 0.0461 +- 0.1142 deg 

<img width="1400" height="600" alt="X_error_figures" src="https://github.com/user-attachments/assets/50dc89cd-973a-46f3-951e-6e166c1edf57" />

### Y-Axis
- Trajectory starts at t = 5.14s 
- -1.3 deg angular offset to account for a 0 error 
- No systematic errors 
    => Hence data analysed in the domain **t = [5.14, 43.14]s** 

=> Deviation = 0.0287 +- 0.8737 deg 

=> |Resolution| = 0.0469 +- 0.0983 deg 

<img width="1400" height="600" alt="Y_error_figures" src="https://github.com/user-attachments/assets/cd5c0bb0-ae04-42ab-81e6-2b4973d0f362" />

### Z-Axis 
- Trajectory starts at t = 4.6083s 
- 0.3806 deg anguler offset to account for a 0 error 
- Systematic error after t = 30.6083s: wires got stuck causing stepper to skip steps 
    => Hence data only analysed in the domain **t = [4.6083, 30.6083]s** to remove systematic error 

=> Deviation = -0.1758 +- 0.1603 deg 

=> |Resolution| = 0.0457 +- 0.0586 deg 

<img width="1400" height="600" alt="Z_error_figures" src="https://github.com/user-attachments/assets/5960a8b3-e2a6-4430-828d-d41547966e8c" />
