# ```/src/``` 

This directory is where all of the code is found! There is a webapp UI where the user can create bespoke trajectories and save them onto a MicroSD card. Then the MicroSD card can be inserted into the Teensy 4.1-based controller board for the High-Tech Fidget Spinner to execute the bespoke trajectories. This directory contains the code for the Teensy 4.1-based controller board and the webapp. 

```
├── firmware/          # Code for the controller board 
├── webapp /           # Code for the web-based UI
└── README.md 
``` 

## ```/src/firmware/``` 
- [```./main/main.ino```](/src/firmware/main/main.ino) is the code that should be flashed onto the controller board 
- [```./test-scripts/](/src/firmware/test-scripts/) contains test code snippets 

## ```/src/webapp/``` 
- [```./static/```](/src/webapp/static/) contains CSS and JS files for the webapp 
- [```./templates/index.html```](/src/webapp/templates/index.html) is the HTML file for the webapp 
- [```./webapp.py```](/src/webapp/webapp.py) is the Python backend for the web UI 

## Using the code 

[```/src/firmware/main/main.ino```](/src/firmware/main/main.ino) can be uploaded to the Teensy 4.1 using the Arduino IDE by following the guide on https://www.pjrc.com/teensy/tutorial.html. 

You should run [```/src/webapp/webapp.py```](/src/webapp/webapp.py) to launch the webapp! ```pip install``` can be used to install any libraries that you don't have. 