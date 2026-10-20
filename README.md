# Electrical Meter Solution

See [conference MVP status](CONFERENCE_MVP.md) for current house-load/price behavior
and the remaining validation/pinning handoff.

## Clone

```bash
cd v6_workspace/

git clone git@github.com:silabs-apnagy/emdemo-e-meter-app.git MatterElectricalMeterSolution
```

## Build
Open the Solution that was cloned into the Studio workspace in Studio 6.
Open the MatterElectricalMeterOverThread.slcp
On the Overview panel in the Project Details panel press the 3 dots ... and select Force Generation to be safe. 
The project should build in VS code. 

## Run
Press PB1 on the WPK to toggle the Utility Price states

Current states: Low - Mid - High (LED lights up)
