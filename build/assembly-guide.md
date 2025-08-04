# Assembly Guide 

To have a functioning High-Tech Fidget Spinner, there are three high-level steps: 
1) Mechanical assembly 
2) Electronic assembly 
3) Flashing firmware 

Completing these three steps may take anywhere between **2 - 4+ hours**. Before starting, please make sure you have all of the materials from [```/build/BoM.md```](/build/BoM.md) and the appropriate tools. 

Let's begin! 

## Mechanical assembly 

The mechanical assembly is split into the following assemblies: 
- Z-frame subassembly 
- Y-frame subassembly 
- X-frame subassembly 

### Z-frame subassembly 
1) Assemble the 4 base pieces together like a jigsaw puzzle! 

![IMG_20250626_222335](https://github.com/user-attachments/assets/c22fc44d-14e3-4102-9e0e-fb7a0f65b19a)

2) Attach the Nema 17 42-60 to the Base Stepper Bracket 

![IMG_20250626_222410](https://github.com/user-attachments/assets/2eaf04f4-5665-40d6-8474-834ece14601f)

3) Attach the E38S6G5-600B-G24 rotary encoder to the Base Rotary Encoder Bracket 

![IMG_20250626_223214](https://github.com/user-attachments/assets/6182707f-6b79-4a86-96d5-ceecaf3cd2da)

4) Mount the Base Stepper Bracket and Base Rotary Encoder Bracket to the base 

![IMG_20250626_224244](https://github.com/user-attachments/assets/69191335-0cc5-4fee-aa59-bf1e55a51556)

5) Attach the 4 Base Posts to the corners of the base 

![IMG_20250626_225404](https://github.com/user-attachments/assets/8ba212ad-1e55-4e3e-ba90-945bfd8abc5f)

6) Secure the turntable bearing on top of the Base Posts 

![IMG_20250626_230224](https://github.com/user-attachments/assets/458da974-46f4-401f-b9e7-9f05eb4e4dac)

7) Fix a shaft coupler to the shaft of the Nema 17 42-60 stepper motor. Fix a GT20 pulley around the shaft of t he E38S6G5-600B-G24 rotary encoder. Loop a 135mm GT20 timing belt around the pulley 

![IMG_20250626_232446](https://github.com/user-attachments/assets/9e3798ac-ff9c-4c50-a5ae-e1fbd5de9f3b)

### Y-frame subassembly 
1) Slot the Y Bracket 1 and Y Bracket 2 into the Z Plate 

![IMG_20250626_235049](https://github.com/user-attachments/assets/2d3c31b8-aa45-4875-a534-5d4dd0a1f987)

2) Add M5 screws and secure Y Bracket 1 and Y Bracket 2 with washers and nuts. Add M5 screws in the centre and around the edges of the Z Plate to fix it to the Z-frame subassembly 

![IMG_20250626_235107](https://github.com/user-attachments/assets/3084bc14-4e23-430c-a398-987e021e479e)

3) Secure a GT20 timing pulley to the middle M5 screw of the Z Plate 

![IMG_20250626_235226](https://github.com/user-attachments/assets/7ed6636e-1338-4331-a878-70d739e7980b)

4) Mount the Y-frame subassembly on top of the Z-frame subassembly and fix using M5 nuts. 

![IMG_20250627_003956](https://github.com/user-attachments/assets/c029c248-4348-4ba1-9ebf-ae8d24f1e5e6)

### X-frame subassembly 
1) Screw the IMU Carriage_Top and IMU Carriage_Bottom together, with the semi-circles on them forming a complete hole. Run M5 screws with toothed washers through the holes 

![IMG_20250627_013600](https://github.com/user-attachments/assets/6c311c91-ee82-4d35-b673-5bc794137ab3)

2) Attach the IMU_Carriage to the X Bracket via the bearing inserts, while including the X Bobbin. Run M5 screws with toothed washers through the centre holes of the X Bracket 

![IMG_20250627_020828](https://github.com/user-attachments/assets/42217a4c-5b03-458a-ad66-b6e3505a48a1)

3) Mount the Nema 17 42-23 and E38S6G5-600B-G24 rotary encoder respectively to the X Bracket and M5 screws of the IMU Carriage via the shaft couplings 

![IMG_20250627_023807](https://github.com/user-attachments/assets/8904618b-9c78-436f-9742-557deda8bf2c)

4) Attach the X Bracket to the Y-frame subassembly via the bearing inserts, while including the Y Bobbin 

![IMG_20250627_024929](https://github.com/user-attachments/assets/ec818831-3130-4590-b5fb-99c30aa359a1)

5) Mount the Nema 17 42-38 and E38S6G5-600B-G24 rotary encoder to their respective Y Brackets 

![IMG_20250627_032512](https://github.com/user-attachments/assets/384d8a6e-a8f4-4395-af44-c2bd219dadad)

## Elelctronic assembly 
Connect the Teensy 4.1, TMC2209 drivers, stepper motors and rotary encoders as shown by the schmatic below: 


All of the components can be connected together on a large breadboard: 


Or a custom PCB can be manufactured using the [GERBER](/build/PCB/GERBER/) files and the components can be soldered onto the PCB, following the board layout 

## Flashing firmware 
1) Download and ensure [```/src/firmware/main/main.ino/```](/src/firmware/main/main.ino) is placed inside a directory called ```/main/``` 
2) Open [```./main.ino/```](/src/firmware/main/main.ino) with the Arduino IDE 
3) Follow [this guide](https://www.pjrc.com/teensy/tutorial.html) to flash the code the a Teensy 4.1 