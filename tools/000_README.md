# ```/tools/``` 

There is a large variety of different stepper motors with different operating parameters. An appropriate combination of stepper motors must be chosen, because for example, the base stepper motor will experience the greatest load. This directory contains a model to output appropriate stepper motor combination from the inertia modelling in [/docs/development-log/PRD.md](/docs/development-log/PRD.md).  

```
├── /stepper-selection-model/
|   ├── stepper_data.csv                    # Different stepper motors' operating data 
|   ├── stepper_options.csv                 # Model output containing feasible stepper combinations 
|   └── stepper-selection-model.py          # Python model to choose approrpiate stepper motors 
└── README.md 
```
