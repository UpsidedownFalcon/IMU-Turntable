import pandas as pd 

data = pd.read_csv('C:/Users/bhavy/OneDrive/Documents/GitHub/IMU-Turntable/Stepper Selection Model/stepper_data.csv') 

selection = ["X", "Y", "Z"] 
feasible_combinations = []

for idx_X, row_X in data.iterrows():
    model_name = row_X["Model"]
    if model_name.startswith("14"):
        #print("\n")
        #print(f"Processing model: {model_name}", idx_X) 

        # Work out required X torque and its OVERESTIMATE (OE) 
        X_Treq = ((2 * data.loc[idx_X, "Rotor Inertia (kgm2)"]) + 1.379E-6) * 5.236 
        OE_X_Treq = 10 * X_Treq
        # If motor has suitable torque, assign it to the position 
        if(data.loc[idx_X, "Holding Torque (Nm)"] > OE_X_Treq): 
            selection[0] = data.loc[idx_X, "Model"] 
            #print(selection) 

            for idx_Y, row_Y in data.iterrows(): 
                # Work out the required Y torque and its OVERESTIMATE (OE) 
                Y_Treq = (((1/12) * ((2 * data.loc[idx_X, "Mass (kg)"]) + 11.86E-3) * (((2 * data.loc[idx_X, "Body Length (m)"]) + (4 * data.loc[idx_X, "Shaft Length (m)"]) + 15.25E-3)**2)) + (2 * data.loc[idx_Y, "Rotor Inertia (kgm2)"])) * 5.236 
                OE_Y_Treq = 10 * Y_Treq 
                # If motor has suitable torque, assign it to the position 
                if(data.loc[idx_Y, "Holding Torque (Nm)"] > OE_Y_Treq): 
                    selection[1] = data.loc[idx_Y, "Model"] 
                    #print(selection)

                    for idx_Z, row_Z in data.iterrows(): 
                        # Work out the required Z torquie and its OVERSTIMATE (OE)
                        Z_Treq = (((1/8) * ((2 * data.loc[idx_X, "Mass (kg)"]) + (2 * data.loc[idx_Y, "Mass (kg)"]) + 11.86E-3) * (((2 * data.loc[idx_X, "Body Length (m)"]) + (4 * data.loc[idx_X, "Shaft Length (m)"]) + 15.25E-3)**2)) + (2 * data.loc[idx_Z, "Rotor Inertia (kgm2)"])) * 5.236 
                        OE_Z_Treq = 10 * Z_Treq 
                        # If motor has suitable torque, assign it to the position 
                        if(data.loc[idx_Z, "Holding Torque (Nm)"] > OE_Z_Treq): 
                            selection[2] = data.loc[idx_Z, "Model"] 
                            print(selection) 
                            feasible_combinations.append(selection.copy())
                        
                        else: 
                            selection[2] = "-"
                            #print(selection) 

                else: 
                    selection[1] = "-" 
                    #print(selection)

        else: 
            selection[0] = "-" 
            #print(selection) 

feasible_combinations = pd.DataFrame(feasible_combinations) 
feasible_combinations.to_csv('C:/Users/bhavy/OneDrive/Documents/GitHub/IMU-Turntable/Stepper Selection Model/stepper_options.csv')
print(feasible_combinations)     