# How to Use the High-Tech Fidget Spinner 

Congratulations! If you have made it this far, you hopefully have a (working) 3-DoF IMU turntable! Here's how to use it: 

1) Insert a MicroSD card into your device - make sure it has **nothing saved onto it** 

2) Within your empty MicroSD card, create an **empty folder** called ```traj``` 

3) Open the webapp UI Python code at [```src/webapp/webapp.py```](/src/webapp/webapp.py) 

4) Replace line 292, ```sd_mount = r"D:\traj"```, with the path to ```traj``` on your MicroSD card 

5) Run the Python script and navigate to the IP address printed in the termainal 

6) You will see an UI which looks like this:

<img width="1264" height="622" alt="009_GUI" src="https://github.com/user-attachments/assets/f238dadc-a307-4140-8eb6-2cd17539aad0" />

The UI transitions between the following stages: 

<img width="1072" height="539" alt="008_Webapp architecture" src="https://github.com/user-attachments/assets/c13a87a6-92eb-491e-8c39-8575f2b79201" />

Through the UI, you can: 
- Change your stepper motor parameters 
- Enter a trajectory for the X, Y and Z axes as piecewise functions 
- Once a trajectory has been entered for **all three** axes, save them onto the MicroSD card 

7) Once trajectories have been saved onto the MicroSD card, remove it from your computer 

8) Navigate to [```/src/firmware/main/main.ino```](/src/firmware/main/main.ino) and modify lines 24 and 25 such that ```STEPS_PER_REV``` and ```MICROSTEPS``` match your stepper motor parameters and stepper driver configuration. Then **upload the firmware** to the controller board 

9) With the controller board **powered off**, insert the MicroSD card into the Teensy 4.1 

10) **First** turn on the +24V DC supply for the stepper motors 

11) **Second**, turn on the +5V DC supply for the Teensy 4.1

12) Et voilà! 

## Bespoke use 

- You can use the universal TPU mounts to attach your own IMU device to the High-Tech Fidget Spinner 

- You can modify [```/src/firmware/main/main.ino```](/src/firmware/main/main.ino) to include bespoke code to synchronously log data from your IMU device to the MicroSD card 
