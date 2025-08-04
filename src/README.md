# ```/src/``` 

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

## Launch the webapp! 
You should run [```/src/webapp/webapp.py```](/src/webapp/webapp.py) to launch the webapp! ```pip install``` can be used to install any libraries that you don't have. 