/*=========================================================================

  Program:   ParaView
  Module:    vtkPVMPIProcessModule.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVMPIProcessModule.h"
#include "vtkObjectFactory.h"
#include "vtkInstantiator.h"

#include "vtkClientServerStream.h"
#include "vtkToolkits.h"
#include "vtkPVConfig.h"
#include "vtkMultiProcessController.h"
#include "vtkPVApplication.h"
#include "vtkDataSet.h"
#include "vtkSource.h"
#include "vtkFloatArray.h"
#include "vtkDoubleArray.h"
#include "vtkCharArray.h"
#include "vtkLongArray.h"
#include "vtkShortArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkMapper.h"
#include "vtkString.h"
#ifdef VTK_USE_MPI
#include "vtkMPIController.h"
#include "vtkMPICommunicator.h"
#include "vtkMPIGroup.h"
#endif

#include "vtkPVPart.h"
#include "vtkPVPartDisplay.h"
#include "vtkPVInformation.h"
#include "vtkClientServerStream.h"
#include "vtkClientServerInterpreter.h"

#define VTK_PV_SLAVE_CLIENTSERVER_RMI_TAG 397529

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVMPIProcessModule);
vtkCxxRevisionMacro(vtkPVMPIProcessModule, "1.24");

int vtkPVMPIProcessModuleCommand(ClientData cd, Tcl_Interp *interp,
                            int argc, char *argv[]);



// external global variable.
vtkMultiProcessController *VTK_PV_UI_CONTROLLER = NULL;


//----------------------------------------------------------------------------
void vtkPVSendStreamToServerNodeRMI(void *localArg, void *remoteArg,
                                    int remoteArgLength,
                                    int vtkNotUsed(remoteProcessId))
{
  vtkPVMPIProcessModule* self =
    reinterpret_cast<vtkPVMPIProcessModule*>(localArg);
  self->GetInterpreter()
    ->ProcessStream(reinterpret_cast<unsigned char*>(remoteArg),
                    remoteArgLength);
}


//----------------------------------------------------------------------------
vtkPVMPIProcessModule::vtkPVMPIProcessModule()
{
  this->Controller = NULL;

  this->ArgumentCount = 0;
  this->Arguments = NULL;
  this->ReturnValue = 0;
}

//----------------------------------------------------------------------------
vtkPVMPIProcessModule::~vtkPVMPIProcessModule()
{
  if (this->Controller)
    {
    this->Controller->Delete();
    this->Controller = NULL;
    }


  this->ArgumentCount = 0;
  this->Arguments = NULL;
  this->ReturnValue = 0;
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "Controller: " << this->Controller << endl;;
}

//----------------------------------------------------------------------------
// Each process starts with this method.  One process is designated as
// "master" and starts the application.  The other processes are slaves to
// the application.
void vtkPVMPIProcessModuleInit(
  vtkMultiProcessController *vtkNotUsed(controller), void *arg )
{
  vtkPVMPIProcessModule *self = (vtkPVMPIProcessModule *)arg;
  self->Initialize();
}


//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::Initialize()
{
  int myId, numProcs;

  myId = this->Controller->GetLocalProcessId();
  numProcs = this->Controller->GetNumberOfProcesses();

#ifdef MPIPROALLOC
  vtkCommunicator::SetUseCopy(1);
#endif

  if (myId ==  0)
    { // The last process is for UI.
    vtkPVApplication *pvApp = this->GetPVApplication();
    // This is for the SGI pipes option.
    pvApp->SetNumberOfPipes(numProcs);

#ifdef PV_HAVE_TRAPS_FOR_SIGNALS
    pvApp->SetupTrapsForSignals(myId);
#endif // PV_HAVE_TRAPS_FOR_SIGNALS

    if (pvApp->GetStartGUI())
      {
      pvApp->Script("wm withdraw .");
      pvApp->Start(this->ArgumentCount,this->Arguments);
      }
    else
      {
      pvApp->Exit();
      }
    this->ReturnValue = pvApp->GetExitStatus();
    }
  else
    {
    this->Controller->AddRMI(vtkPVSendStreamToServerNodeRMI, this,
                             VTK_PV_SLAVE_CLIENTSERVER_RMI_TAG);
    this->Controller->ProcessRMIs();
    }
}

//----------------------------------------------------------------------------
int vtkPVMPIProcessModule::Start(int argc, char **argv)
{
  // Initialize the MPI controller.
  this->Controller = vtkMultiProcessController::New();
  this->Controller->Initialize(&argc, &argv, 1);
  vtkMultiProcessController::SetGlobalController(this->Controller);

  if (this->Controller->GetNumberOfProcesses() > 1)
    { // !!!!! For unix, this was done when MPI was defined (even for 1
      // process). !!!!!
    this->Controller->CreateOutputWindow();
    }
  this->ArgumentCount = argc;
  this->Arguments = argv;

  // Go through the motions.
  // This indirection is not really necessary and is just to mimick the
  // threaded controller.
  this->Controller->SetSingleMethod(vtkPVMPIProcessModuleInit,(void*)(this));
  this->Controller->SingleMethodExecute();

  this->Controller->Finalize(1);

  return this->ReturnValue;
}


//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::Exit()
{
  int id, myId, num;

  // Send a break RMI to each of the slaves.
  num = this->Controller->GetNumberOfProcesses();
  myId = this->Controller->GetLocalProcessId();
  for (id = 0; id < num; ++id)
    {
    if (id != myId)
      {
      this->Controller->TriggerRMI(id,
                                   vtkMultiProcessController::BREAK_RMI_TAG);
      }
    }
}

//----------------------------------------------------------------------------
int vtkPVMPIProcessModule::GetPartitionId()
{
  if (this->Controller)
    {
    return this->Controller->GetLocalProcessId();
    }
  return 0;
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToClient()
{
  this->Interpreter->ProcessStream(*this->ClientServerStream);
  this->ClientServerStream->Reset();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToServer()
{
  this->SendStreamToServerInternal();
  this->ClientServerStream->Reset();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToRenderServer()
{
  this->SendStreamToServer();
}


//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToServerRoot()
{
  this->Interpreter->ProcessStream(*this->ClientServerStream);
  this->ClientServerStream->Reset();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToRenderServerRoot()
{ 
  this->SendStreamToServer();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToClientAndServer()
{
  this->SendStreamToServer();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToClientAndServerRoot()
{
  this->SendStreamToServerRoot();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToRenderServerAndServerRoot()
{
  this->SendStreamToServerRoot();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToClientAndRenderServerRoot()
{
  this->SendStreamToClientAndServerRoot();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToClientAndRenderServer()
{
  this->SendStreamToClientAndServer();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToRenderServerAndServer()
{
  this->SendStreamToServer();
}

//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToRenderServerClientAndServer()
{
  this->SendStreamToClientAndServer();
}



//----------------------------------------------------------------------------
void vtkPVMPIProcessModule::SendStreamToServerInternal()
{
  // First send the command to the other server nodes.
  int numPartitions = this->GetNumberOfPartitions();
  for(int i=1; i < numPartitions; ++i)
    {
    this->SendStreamToServerNodeInternal(i);
    }

  // Now process the stream locally.
  this->Interpreter->ProcessStream(*this->ClientServerStream);
}

//----------------------------------------------------------------------------
void
vtkPVMPIProcessModule::SendStreamToServerNodeInternal(int remoteId)
{
  if(remoteId == this->Controller->GetLocalProcessId())
    {
    this->Interpreter->ProcessStream(*this->ClientServerStream);
    }
  else
    {
    const unsigned char* data;
    size_t length;
    this->ClientServerStream->GetData(&data, &length);
    this->Controller->TriggerRMI(remoteId, (void*)data, length,
                                 VTK_PV_SLAVE_CLIENTSERVER_RMI_TAG);
    }
}

//----------------------------------------------------------------------------
int vtkPVMPIProcessModule::GetNumberOfPartitions()
{
  if (this->Controller)
    {
    return this->Controller->GetNumberOfProcesses();
    }
  return 1;
}



//----------------------------------------------------------------------------
// This method is broadcast to all processes.
void
vtkPVMPIProcessModule::GatherInformationInternal(const char* infoClassName,
                                                 vtkObject *object)
{
  vtkClientServerStream css;
  int myId = this->Controller->GetLocalProcessId();

  if (object == NULL)
    {
    vtkErrorMacro("Object id must be wrong.");
    return;
    }

  // Create a temporary information object.
  vtkObject* o = vtkInstantiator::CreateInstance(infoClassName);
  vtkPVInformation* tmpInfo = vtkPVInformation::SafeDownCast(o);
  if (tmpInfo == NULL)
    {
    vtkErrorMacro("Could not create information object " << infoClassName);
    if (o) { o->Delete(); }
    // How to exit without blocking ???
    return;
    }
  o = NULL;

  if(myId != 0)
    {
    if(tmpInfo->GetRootOnly())
      {
      // Root-only and we are not the root.  Do nothing.
      tmpInfo->Delete();
      return;
      }
    tmpInfo->CopyFromObject(object);
    tmpInfo->CopyToStream(&css);
    size_t length;
    const unsigned char* data;
    css.GetData(&data, &length);
    int len = static_cast<int>(length);
    this->Controller->Send(&len, 1, 0, 498798);
    this->Controller->Send(const_cast<unsigned char*>(data), len, 0, 498799);
    tmpInfo->Delete();
    return;
    }

  // This is node 0.  First get our own information.
  this->TemporaryInformation->CopyFromObject(object);

  if(!tmpInfo->GetRootOnly())
    {
    // Merge information from other nodes.
    int numProcs = this->Controller->GetNumberOfProcesses();
    for(int idx = 1; idx < numProcs; ++idx)
      {
      int length;
      this->Controller->Receive(&length, 1, idx, 498798);
      unsigned char* data = new unsigned char[length];
      this->Controller->Receive(data, length, idx, 498799);
      css.SetData(data, length);
      tmpInfo->CopyFromStream(&css);
      this->TemporaryInformation->AddInformation(tmpInfo);
      delete [] data;
      }
    }
  tmpInfo->Delete();
}

//----------------------------------------------------------------------------
#ifdef VTK_USE_MPI
int vtkPVMPIProcessModule::LoadModuleInternal(const char* name)
{
  // Make sure we have a communicator.
  vtkMPICommunicator* communicator = vtkMPICommunicator::SafeDownCast(
    this->Controller->GetCommunicator());
  if(!communicator)
    {
    return 0;
    }

  // Try to load the module on the local process.
  int localResult = this->Interpreter->Load(name);

  // Gather load results to process 0.
  int numProcs = this->Controller->GetNumberOfProcesses();
  int myid = this->Controller->GetLocalProcessId();
  int* results = new int[numProcs];
  communicator->Gather(&localResult, results, numProcs, 0);

  int globalResult = 1;
  if(myid == 0)
    {
    for(int i=0; i < numProcs; ++i)
      {
      if(!results[i])
        {
        globalResult = 0;
        }
      }
    }

  delete [] results;

  return globalResult;
}
#else
int vtkPVMPIProcessModule::LoadModuleInternal(const char* name)
{
  return this->Superclass::LoadModuleInternal(name);
}
#endif
