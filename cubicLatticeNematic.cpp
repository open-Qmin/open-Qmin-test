#include "std_include.h" // std library includes, definition of scalar, etc.. has a "using namespace std" in it, because I'm lazy

//we'll use TCLAP as our command line parser
#include <tclap/CmdLine.h>
#include "cuda_profiler_api.h"

#include "functions.h"
#include "gpuarray.h"
#include "simulation.h"
#include "qTensorLatticeModel.h"
#include "nematicInteraction.h"
#include "energyMinimizerFIRE.h"
#include "energyMinimizerAdam.h"
#include "noiseSource.h"
#include "indexer.h"

using namespace std;
using namespace TCLAP;

/*!
Eventually: run minimization of a (3D) cubic lattice with a local Q-tensor
living on each lattice site, together with some sites enforcing boundary
conditions.

Will require that the compiled dimension in the CMakeList be 5
*/
int main(int argc, char*argv[])
{
    // wrap the command line parser in a try block...
    try
    {
    //First, we set up a basic command line parser with some message and version
    CmdLine cmd("dDimSim applied to a lattice of XY-spins", ' ', "V0.1");

    //define the various command line strings that can be passed in...
    //ValueArg<T> variableName("shortflag","longFlag","description",required or not, default value,"value type",CmdLine object to add to
    ValueArg<int> programSwitchArg("z","programSwitch","an integer controlling program branch",false,0,"int",cmd);
    ValueArg<int> gpuSwitchArg("g","USEGPU","an integer controlling which gpu to use... g < 0 uses the cpu",false,-1,"int",cmd);
    ValueArg<int> maxIterationsSwitchArg("i","iterations","number of timestep iterations",false,100,"int",cmd);
    ValueArg<scalar> lengthSwitchArg("l","sideLength","size of simulation domain",false,10.0,"double",cmd);

    //parse the arguments
    cmd.parse( argc, argv );

    //define variables that correspond to the command line parameters
    int programSwitch = programSwitchArg.getValue();
    int maximumIterations = maxIterationsSwitchArg.getValue();
    scalar L = lengthSwitchArg.getValue();

    int gpuSwitch = gpuSwitchArg.getValue();
    bool GPU = false;
    if(gpuSwitch >=0)
        GPU = chooseGPU(gpuSwitch);

    int dim =DIMENSION;

    if(DIMENSION != 5)
        {
        cout << "oopsies. you're doing an q-tensor model with improperly defined dimenisonality. rethink your life choices" << endl;
        throw std::exception();
        }

    scalar a = 1;
    scalar b = 10;
    scalar c = 1;
    scalar l = 1;
    scalar S0 = (-b+sqrt(b*b-24*a*c))/(6*c);
    noiseSource noise(true);
    shared_ptr<qTensorLatticeModel> Configuration = make_shared<qTensorLatticeModel>(L);
    Configuration->setNematicQTensorRandomly(noise,S0);

    shared_ptr<Simulation> sim = make_shared<Simulation>();
    sim->setConfiguration(Configuration);

    shared_ptr<nematicInteraction> nematic = make_shared<nematicInteraction>(a,b,c,l);
    nematic->setModel(Configuration);
    sim->addForce(nematic);



    //after the simulation box has been set, we can set particle positions... putting this here ensures that the random
    //spins are the same for gpu and cpu testing
    if(gpuSwitch >=0)
        {
        sim->setCPUOperation(false);
        };



    shared_ptr<energyMinimizerFIRE> fire = make_shared<energyMinimizerFIRE>(Configuration);
    fire->setFIREParameters(0.05,0.99,0.1,1.1,0.95,.9,4,1e-12);
    fire->setMaximumIterations(maximumIterations);
    shared_ptr<energyMinimizerAdam> adam  = make_shared<energyMinimizerAdam>();
    adam->setAdamParameters();
    adam->setMaximumIterations(maximumIterations);
    if(programSwitch ==1)
        sim->addUpdater(fire,Configuration);
    else
        sim->addUpdater(adam,Configuration);

    if(gpuSwitch >=0)
        {
        sim->setCPUOperation(false);
        };
    scalar E = sim->computePotentialEnergy();
    printf("simulation potential energy at %f\n",E);

    dVec meanSpin = Configuration->averagePosition();
    cout << "average spin magnitude "<< norm(meanSpin) << endl;
    printdVec(meanSpin);

    int curMaxIt = maximumIterations;

    clock_t t1 = clock();
    cudaProfilerStart();
    sim->performTimestep();
    clock_t t2 = clock();
    cudaProfilerStop();
    meanSpin = Configuration->averagePosition();
    cout << "average spin magnitude "<< norm(meanSpin);
    printdVec(meanSpin);

    cout << endl << "minimization took " << (t2-t1)/(scalar)CLOCKS_PER_SEC << endl;
    sim->setCPUOperation(true);
    E = sim->computePotentialEnergy();
    printf("simulation potential energy at %f\n",E);

//
//The end of the tclap try
//
    } catch (ArgException &e)  // catch any exceptions
    { cerr << "error: " << e.error() << " for arg " << e.argId() << endl; }
    return 0;
};