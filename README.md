# High-Tech Fidget Spinner V2: "Treexelator-PRO-T2000"


Welcome! 

This is an open-source project for the construction of a 3-DoF IMU turntable, its the second version of the "High-Tech Fidget Spinner". our main goal is to increase the sensitivity of the testing device and make it more accurate while keeping the cost down as low as possible. 
Our goal is to have 0.001 degree of sensitivity. 
This is a ongoing project and will be updated...

Approximate Timeline

<img width="1676" height="205" alt="image" src="https://github.com/user-attachments/assets/edeb5be1-190e-4a15-9876-c2f793c4f1dd" />

Captain's Journal:

18/10/2025: Did a prelimery research about which components to use, tought about using REV robotics sparc flex + neo vortex combo due to their lib support and internal decoder inside the esc, decided to use CAN bus to communicate.

22/10/2025: Did more research and understood the spark flex and neo are not a good choise for this device, their quality is mehhh, and the max bit support of spark flec is around 200000 increments per sec which is nothing with high sensitivity encoder and some speed, decided to look at other compoennts and my research directed me towards Odrive Pro ESC

23/10/2025: Designed a basic mass model for the device and calculated the torque needed to accelerate it at all axeses from 0 rpm to 60 rpm in 1 second, the report can be found at github site, then did a research for motors and found cubemars gl series, they are desiged for gimbal use with butter smooth operation, altough pricey, they were a really good match.

2/11/2025: Did some research for the microcontrollers, the Odrive Pros have stm32H7s as their microcontroller, after some research about STM32H7 microchip, I realized it was much better than any esp32 ı could use and decided to use STM32H723, it support ethernet, I3C, I2C, Uart, SPI, Usart, CAN, SAI, QSPI etc, as this is a machine to test different sensors, this microcontroller can comminicate with anything ı threw at.

3/11/2025: Opened this branch after getting approved by the owner.

5/11/2025: Ordered (Most) the part that are in the BoM (Bill of Materials), please find atteched the BoM, the datasheets, model numbers, URL to buy, descriptions can be found.

6/11/2025: Ordered more parts.

8/11/2025-12/11/2025: Some part have arrived still waiting for the slip rings, aksim encoders, and need to order bearings.

10/11/2025: Started designing the 3rd axis.

14/11/2025: read few document about acl testing for the injury sim jig, had more idea about what to do.

15/11/2025: Finished Designing the 3rd axis and submittedi it.

20/11/2025: Started doing research on the tolorances we have for the mechanical parts and different manufacturning methods.

23/11/2025: did more research about tolorances usign GD&T, started writing the report for manufacturing.

22/11/2025: contacted DHL and RLS for products and shipment, made few calls with DHL, solved a problem with their system, RLS changed prices, asked them to understand if we can get our price diff back, (Note: no reply:( 

26/11/2025: submitted the GD&T report to higher ups. 

27/11/2025: started to do the manufacturing report about the types of manufacturing we can use and their prices.

01/12/2025: finished doing the materials/manufacturing report, can be accessed in github.

5/12/2025: started doing the 2nd and 3rd axes, the design is being done using fusion 360. 

8/12/2025: finished the V1 of the desing, all ground plate, 1st axis, 2nd axis, 3rd axis, and the sensor plate is ready for 3d prinitng for prototyping. 3d printing is only for model and does not have the tolorances we need. 
