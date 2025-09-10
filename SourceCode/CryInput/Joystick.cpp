//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Joystick.cpp
//  Description: joystick function support
//
//	History:
//	-Jan 31,2001:Created by Marco Corbetta
//  -March 2005: NickH: Refactored, ported to direct input, and
//               added Pressed,Down,Released functionality.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <string.h>
#include <stdio.h>
#include <ILog.h>
#include "Joystick.h"

#ifdef WIN32
#include "Mmsystem.h"
#pragma comment(lib, "winmm.lib")
#endif


#ifdef _DEBUG
static char THIS_FILE[] = __FILE__;
#define DEBUG_CLIENTBLOCK new( _NORMAL_BLOCK, THIS_FILE, __LINE__) 
#define new DEBUG_CLIENTBLOCK
#endif

#define MAX_BUTTON    32

StickState & StickState::operator=(const StickState &state)
{
  m_buttons=state.m_buttons;
  memcpy(m_dirs,state.m_dirs,sizeof(m_dirs));
  memcpy(m_hatdirs,state.m_hatdirs,sizeof(m_hatdirs));

  m_vAnalog1Dir.x=state.m_vAnalog1Dir.x;
  m_vAnalog1Dir.y=state.m_vAnalog1Dir.y;
  m_vAnalog1Dir.z=state.m_vAnalog1Dir.z;
  
  m_vAnalog2Dir.x=state.m_vAnalog2Dir.x;
  m_vAnalog2Dir.y=state.m_vAnalog2Dir.y;
  m_vAnalog2Dir.z=state.m_vAnalog2Dir.z;

  return *this;
}

void StickState::ClearState()
{
  m_buttons=0;

  memset(m_dirs,0,sizeof(m_dirs));
  memset(m_hatdirs,0,sizeof(m_hatdirs));

  m_vAnalog1Dir.Set(0,0,0);
  m_vAnalog2Dir.Set(0,0,0);
}



//////////////////////////////////////////////////////////////////////
CJoystick::CJoystick()
{
	m_pLog=NULL;

  //m_apSticks=0;

	//memset(m_dirs,0,sizeof(m_dirs));
	//memset(m_buttons,0,sizeof(m_buttons));
	//memset(m_hatdirs,0,sizeof(m_hatdirs));

	m_joytime=0;
	m_numjoysticks = 0;

	m_initialized=false;
}

CJoystick::~CJoystick()
{
  VSticksIter itStick,itStickEnd;

  if(!m_vSticks.empty())
  {
    for(itStick=m_vSticks.begin(),itStickEnd=m_vSticks.end();itStick!=itStickEnd;++itStick)
    {
      delete *itStick;
    }
  }
}

#ifdef WIN32

///////////////////////////////////////////
BOOL CALLBACK Joy_DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi,LPVOID pvRef)
{
  CJoystick *pJ=(CJoystick *)pvRef;

  if(pJ)
    pJ->AddJoyDI(lpddi);

  return DIENUM_CONTINUE;
}

void CJoystick::AddJoyDI(LPCDIDEVICEINSTANCE lpddi)
{
  HRESULT hr;
  LPDIRECTINPUTDEVICE8 pDIJoy;

  hr=m_pDI->CreateDevice(lpddi->guidInstance,&pDIJoy,0);
  if(FAILED(hr))
    return;
  
  // Set cooperative level.
  hr = pDIJoy->SetCooperativeLevel(m_hWnd,DISCL_EXCLUSIVE|DISCL_BACKGROUND);//DISCL_EXCLUSIVE|DISCL_FOREGROUND);
  if (FAILED(hr))
    return;

  // set game data format
  hr = pDIJoy->SetDataFormat(&c_dfDIJoystick);
  if (FAILED(hr))
    return;

  // Ok, we've got the needed mode and data format...
  // now find out what the joystick can do

  hr = pDIJoy->Poll();

  if (FAILED(hr)) 
  {
    hr = pDIJoy->Acquire();
    while(hr == DIERR_INPUTLOST)    // *** THIS NEEDS FIXING ***  
    {
      hr = pDIJoy->Acquire();
    }
  }

  DIDEVCAPS caps;
  caps.dwSize = sizeof(DIDEVCAPS);
  hr=pDIJoy->GetCapabilities(&caps);
  if (FAILED(hr))
    return;

  Stick *pStick=new Stick();

  pStick->m_numbuttons=caps.dwButtons;
  pStick->m_hatswitch=(caps.dwPOVs>0);

  // Add the stick to our list
  pStick->m_pDIStick=pDIJoy;
  pStick->m_bInit=true;
  m_vSticks.push_back(pStick);
  ++m_numjoysticks;

  m_pLog->LogToFile("Num joy buttons=%d\n", pStick->m_numbuttons);
  //m_pLog->LogToFile("Joystick name=%s\n", joycaps.szPname);
  if(pStick->m_hatswitch)
    m_pLog->LogToFile("Joystick has hatswitch\n");
}

bool CJoystick::InitJoyDI()
{
  HRESULT hr;
  bool bRet=false;

  hr = m_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, Joy_DIEnumDevicesCallback,(void*)this,DIEDFL_FORCEFEEDBACK|DIEDFL_ATTACHEDONLY);

  if (FAILED(hr))
  {    
    // No force-feedback joystick available; take appropriate action. 
  }
  else
  {
    bRet=true;
  }

  return bRet;
}
// Ready to be templated...
class CErrorMsg
{
public:
  typedef HRESULT TErr;
  typedef const char * TMsg;

  struct _TErrorMsg { CErrorMsg::TErr err; CErrorMsg::TMsg msg; };
  
  // Get a message string for the error. This is NOT a fast way to check for errors!
  static bool GetMsg(CErrorMsg::TErr err, CErrorMsg::TMsg *pMsg);
protected:
  
  static _TErrorMsg *m_pMsgTable;
  static TErr m_endOfTable;
};

bool CErrorMsg::GetMsg(CErrorMsg::TErr err,CErrorMsg::TMsg *pMsg)
{
  CErrorMsg::_TErrorMsg *pI;
  *pMsg=(CErrorMsg::TMsg)0;
  TErr end=m_endOfTable;
  bool bRet=false;
  for(pI=m_pMsgTable ; pI->err!=end ; ++pI )
  {
    if(pI->err==err)
    {
      bRet=true;
      *pMsg=pI->msg;
      break;
    }
  }
  return bRet;
}

//---
// Paste the message definitions from MSDN into the source,
// Remove any non-error entries e.g. DI_OK. You might want to use one of these as the end of table marker.
// then open the search-replace dialog, select use regular expressions and selection only
// then paste the search and replace terms from below into the dialog
// then SELECT the text to replace (from the line before to the line after), and hit replace all
//---
// search and replace this (format of error code - string entries in at least the direct input MSDN pages):
// word\n paragraph\n\n  => { word, "word: " "paragraph" },\n
//^[\t ]*{:i}[\t ]*\n[ \t]*{[^\n]*}\n
//\t{ \1,\t"\1: " "\2" },
//--
// Direct Input error return codes
static CErrorMsg::_TErrorMsg apErrorMsg[]=
{
	{ DI_BUFFEROVERFLOW,	"DI_BUFFEROVERFLOW: " "The device buffer overflowed and some input was lost. This value is equal to the S_FALSE standard COM return value. " },
	{ DI_DOWNLOADSKIPPED,	"DI_DOWNLOADSKIPPED: " "The parameters of the effect were successfully updated, but the effect could not be downloaded because the associated device was not acquired in exclusive mode. " },
	{ DI_EFFECTRESTARTED,	"DI_EFFECTRESTARTED: " "The effect was stopped, the parameters were updated, and the effect was restarted. " },
	{ DI_NOEFFECT,	"DI_NOEFFECT: " "The operation had no effect. This value is equal to the S_FALSE standard Component Object Model (COM) return value. " },
	{ DI_NOTATTACHED,	"DI_NOTATTACHED: " "The device exists but is not currently attached to the user's computer. This value is equal to the S_FALSE standard COM return value. " },
	{ DI_POLLEDDEVICE,	"DI_POLLEDDEVICE: " "The device is a polled device. As a result, device buffering does not collect any data and event notifications is not signaled until the IDirectInputDevice8::Poll method is called. " },
	{ DI_PROPNOEFFECT,	"DI_PROPNOEFFECT: " "The change in device properties had no effect. This value is equal to the S_FALSE standard COM return value. " },
	{ DI_SETTINGSNOTSAVED,	"DI_SETTINGSNOTSAVED: " "The action map was applied to the device, but the settings could not be saved. " },
	{ DI_TRUNCATED,	"DI_TRUNCATED: " "The parameters of the effect were successfully updated, but some of them were beyond the capabilities of the device and were truncated to the nearest supported value. " },
	{ DI_TRUNCATEDANDRESTARTED,	"DI_TRUNCATEDANDRESTARTED: " "Equal to DI_EFFECTRESTARTED | DI_TRUNCATED. " },
	{ DI_WRITEPROTECT,	"DI_WRITEPROTECT: " "A SUCCESS code indicating that settings cannot be modified. " },
	{ DIERR_ACQUIRED,	"DIERR_ACQUIRED: " "The operation cannot be performed while the device is acquired. " },
	{ DIERR_ALREADYINITIALIZED,	"DIERR_ALREADYINITIALIZED: " "This object is already initialized " },
	{ DIERR_BADDRIVERVER,	"DIERR_BADDRIVERVER: " "The object could not be created due to an incompatible driver version or mismatched or incomplete driver components. " },
	{ DIERR_BETADIRECTINPUTVERSION,	"DIERR_BETADIRECTINPUTVERSION: " "The application was written for an unsupported prerelease version of DirectInput. " },
	{ DIERR_DEVICEFULL,	"DIERR_DEVICEFULL: " "The device is full. " },
	{ DIERR_DEVICENOTREG,	"DIERR_DEVICENOTREG: " "The device or device instance is not registered with DirectInput. This value is equal to the REGDB_E_CLASSNOTREG standard COM return value. " },
	{ DIERR_EFFECTPLAYING,	"DIERR_EFFECTPLAYING: " "The parameters were updated in memory but were not downloaded to the device because the device does not support updating an effect while it is still playing. " },
	{ DIERR_GENERIC,	"DIERR_GENERIC: " "An undetermined error occurred inside the DirectInput subsystem. This value is equal to the E_FAIL standard COM return value. " },
	{ DIERR_HANDLEEXISTS,	"DIERR_HANDLEEXISTS: " "The device already has an event notification associated with it. This value is equal to the E_ACCESSDENIED standard COM return value. " },
	{ DIERR_HASEFFECTS,	"DIERR_HASEFFECTS: " "The device cannot be reinitialized because effects are attached to it. " },
	{ DIERR_INCOMPLETEEFFECT,	"DIERR_INCOMPLETEEFFECT: " "The effect could not be downloaded because essential information is missing. For example, no axes have been associated with the effect, or no type-specific information has been supplied. " },
	{ DIERR_INPUTLOST,	"DIERR_INPUTLOST: " "Access to the input device has been lost. It must be reacquired. " },
	{ DIERR_INVALIDPARAM,	"DIERR_INVALIDPARAM: " "An invalid parameter was passed to the returning function, or the object was not in a state that permitted the function to be called. This value is equal to the E_INVALIDARG standard COM return value. " },
	{ DIERR_MAPFILEFAIL,	"DIERR_MAPFILEFAIL: " "An error has occurred either reading the vendor-supplied action-mapping file for the device or reading or writing the user configuration mapping file for the device. " },
	{ DIERR_MOREDATA,	"DIERR_MOREDATA: " "Not all the requested information fit into the buffer. " },
	{ DIERR_NOAGGREGATION,	"DIERR_NOAGGREGATION: " "This object does not support aggregation. " },
	{ DIERR_NOINTERFACE,	"DIERR_NOINTERFACE: " "The object does not support the specified interface. This value is equal to the E_NOINTERFACE standard COM return value. " },
	{ DIERR_NOTACQUIRED,	"DIERR_NOTACQUIRED: " "The operation cannot be performed unless the device is acquired. " },
	{ DIERR_NOTBUFFERED,	"DIERR_NOTBUFFERED: " "The device is not buffered. Set the DIPROP_BUFFERSIZE property to enable buffering. " },
	{ DIERR_NOTDOWNLOADED,	"DIERR_NOTDOWNLOADED: " "The effect is not downloaded. " },
	{ DIERR_NOTEXCLUSIVEACQUIRED,	"DIERR_NOTEXCLUSIVEACQUIRED: " "The operation cannot be performed unless the device is acquired in DISCL_EXCLUSIVE mode. " },
	{ DIERR_NOTFOUND,	"DIERR_NOTFOUND: " "The requested object does not exist. " },
	{ DIERR_NOTINITIALIZED,	"DIERR_NOTINITIALIZED: " "This object has not been initialized. " },
	{ DIERR_OBJECTNOTFOUND,	"DIERR_OBJECTNOTFOUND: " "The requested object does not exist. " },
	{ DIERR_OLDDIRECTINPUTVERSION,	"DIERR_OLDDIRECTINPUTVERSION: " "The application requires a newer version of DirectInput. " },
	{ DIERR_OTHERAPPHASPRIO,	"DIERR_OTHERAPPHASPRIO: " "Another application has a higher priority level, preventing this call from succeeding. This value is equal to the E_ACCESSDENIED standard DirectInput return value. This error can be returned when an application has only foreground access to a device but is attempting to acquire the device while in the background. " },
	{ DIERR_OUTOFMEMORY,	"DIERR_OUTOFMEMORY: " "The DirectInput subsystem could not allocate sufficient memory to complete the call. This value is equal to the E_OUTOFMEMORY standard COM return value. " },
	{ DIERR_READONLY,	"DIERR_READONLY: " "The specified property cannot be changed. This value is equal to the E_ACCESSDENIED standard COM return value. " },
	{ DIERR_REPORTFULL,	"DIERR_REPORTFULL: " "More information was requested to be sent than can be sent to the device. " },
	{ DIERR_UNPLUGGED,	"DIERR_UNPLUGGED: " "The operation could not be completed because the device is not plugged in. " },
	{ DIERR_UNSUPPORTED,	"DIERR_UNSUPPORTED: " "The function called is not supported at this time. This value is equal to the E_NOTIMPL standard COM return value. " },
	{ E_HANDLE,	"E_HANDLE: " "The HWND parameter is not a valid top-level window that belongs to the process. " },
	{ E_PENDING,	"E_PENDING: " "Data is not yet available. " },
	{ E_POINTER,	"E_POINTER: " "An invalid pointer, usually NULL, was passed as a parameter." },
  { DI_OK,0 }
};

CErrorMsg::TErr CErrorMsg::m_endOfTable=DI_OK;
CErrorMsg::_TErrorMsg * CErrorMsg::m_pMsgTable=apErrorMsg;


void LogDInputError(ILog *pLog,HRESULT hr,const char *psHeader=0)
{
  const char *psMsg=0;
  if(FAILED(hr))
  {
    if(CErrorMsg::GetMsg(hr,&psMsg))
    {    
    }
    else
    {
      psMsg="Unknown";
    }
    pLog->Log("%s:(%x)'%s'\n",psHeader ? psHeader : "",hr,psMsg);
  }
}

bool CJoystick::PollDI(Stick *pStick)
{
  bool bRet=true;
  HRESULT hr;

  hr = pStick->m_pDIStick->Poll(); 
  
  if (FAILED(hr)) 
  {
    LogDInputError(m_pLog,hr,"PollDI:Poll:");

    if(hr==DIERR_UNPLUGGED)
    {
      pStick->m_bInit=false;
    }

    hr = pStick->m_pDIStick->Acquire();
    while(hr == DIERR_INPUTLOST)    // *** THIS NEEDS FIXING ***  
    {
      hr = pStick->m_pDIStick->Acquire();
    }
    if(FAILED(hr))
      LogDInputError(m_pLog,hr,"PollDI:Acquire:");
    if(hr==DIERR_OTHERAPPHASPRIO)
    {
      static int iCount=0;

      if( (iCount/(30*5))<20 )  // no more than 20 messages
      {
        if(0==(iCount%(30*5)))  // roughly every 5-15 seconds?
          m_pLog->Log("CJoystick::PollDI: DIERR_OTHERAPPHASPRIO\n");
        ++iCount;
      }
    }
  }

  if(FAILED(hr))
  {
    if(hr==DIERR_UNPLUGGED)
    {
      pStick->m_bInit=false;
    }
    bRet=false;
  }

  return bRet;
}

#endif // #ifdef WIN32	

#ifdef WIN32
  bool CJoystick::Init(LPDIRECTINPUT8 lpdi,HINSTANCE hInst,HWND hWnd,ILog *pLog)
#else
  bool CJoystick::Init(ILog *pLog)
#endif
{
	m_pLog=pLog;

  if(!m_initialized)
  {
	//WINDOWS SPECIFIC INIT
#ifdef WIN32	
  m_pDI=lpdi;//pDI;
  m_hInst=hInst;
  m_hWnd=hWnd;

  if(InitJoyDI())
  {
    m_initialized=(m_numjoysticks>0);
    m_pLog->LogToFile("Found %d joystick devices\n", m_numjoysticks);
  }
  if(m_numjoysticks<=0)
  {
    m_initialized=false;
	  m_pLog->LogToFile("Cannot find joystick device\n");	
  }

#endif
  }
  return m_initialized;
}
int volatile dbgHack=0;

///////////////////////////////////////////
void CJoystick::Update()
{	

	//Check if joystick was initialized

	if (!m_initialized) 
		return;

	//WARNING remember
	//use a time check like this one
	//do not call the update every frame
	//
	//float currtime=CSystem::GetTimer()->GetCurrTime();
	//if (currtime-m_joytime<JOY_UPDATE_TIME) 
	//	return;	

	//m_joytime=currtime;


  //int iStick;
  VSticksIter itStick,itStickEnd;

  for(itStick=m_vSticks.begin(),itStickEnd=m_vSticks.end();itStick!=itStickEnd;++itStick)
  {
    Stick *pStick=*itStick;

    if(!pStick || !pStick->m_bInit)
      continue;

#ifdef WIN32	
	  /* get the current status */

    // Save the old state for press/release detection
    pStick->SaveState();

    if(!PollDI(pStick))   // Acquire and poll the stick.
      continue;

    DIJOYSTATE joyState;
    HRESULT hr;
    hr = pStick->m_pDIStick->GetDeviceState(sizeof(DIJOYSTATE), &joyState);
    if (FAILED(hr))
      continue; 

    // Clear the current state ready
    pStick->ClearCurrentState();

    
    // translate the analog axis positions into digital on/off states. Useful for gui's.
	  if (joyState.lX < 16384)
		  pStick->state.m_dirs[JOY_DIR_LEFT] = 1;
	  else 
	  if (joyState.lX> 65535-16384)	
		  pStick->state.m_dirs[JOY_DIR_RIGHT] = 1;

	  if (joyState.lY < 16384)			
		  pStick->state.m_dirs[JOY_DIR_UP] = 1;
	  else 
	  if(joyState.lY > 65535-16384)	
		  pStick->state.m_dirs[JOY_DIR_DOWN] = 1;

    // Get the POV hat state.
	  if (pStick->m_hatswitch)
	  {
		  //memset(m_hatdirs,0,sizeof(m_hatdirs));	
		  if (joyState.rgdwPOV[0] == JOY_POVLEFT)		  pStick->state.m_hatdirs[JOY_DIR_LEFT]=1;
		  if (joyState.rgdwPOV[0] == JOY_POVRIGHT)		pStick->state.m_hatdirs[JOY_DIR_RIGHT]=1;
		  if (joyState.rgdwPOV[0] == JOY_POVFORWARD)	pStick->state.m_hatdirs[JOY_DIR_UP]=1;		
		  if (joyState.rgdwPOV[0] == JOY_POVBACKWARD)	pStick->state.m_hatdirs[JOY_DIR_DOWN]=1;		
	  }

    // Get the digital button states
    pStick->state.m_buttons=0;
    int iBtn;
    for(iBtn=0;iBtn<32;++iBtn)
      pStick->state.m_buttons|=((joyState.rgbButtons[iBtn]>>7)&1)<<iBtn;  // NB: buttons 31 & 32 mapped to triggers below.

	  // (this is a bit ugly, but the input system doesn't deserve better)
	  int iAnalog1X = joyState.lX - 32768;
	  int iAnalog1Y = joyState.lY - 32768;
    //int iAnalog1Z = joyState.lZ - 32768;
    int iAnalog1Z=joyState.rglSlider[0];

	  int iAnalog2X = joyState.lZ - 32768;
	  int iAnalog2Y = joyState.lRz - 32768;
    //int iAnalog2Z = joyState.lRz;// - 32768;
    int iAnalog2Z=joyState.rglSlider[1];

    // Hard coded dead zone.
	  if (abs(iAnalog1X) < 100) iAnalog1X = 0;
	  if (abs(iAnalog1Y) < 100) iAnalog1Y = 0;
    if (abs(iAnalog1Z) < 200) iAnalog1Z = 0;

	  if (abs(iAnalog2X) < 100) iAnalog2X = 0;
	  if (abs(iAnalog2Y) < 100) iAnalog2Y = 0;
    if (abs(iAnalog2Z) < 200) iAnalog2Z = 0;

    // translate and store the analogue joystick axis values.
	  pStick->state.m_vAnalog1Dir.x = iAnalog1X/32768.0f;
	  pStick->state.m_vAnalog1Dir.y = iAnalog1Y/32768.0f;
	  //pStick->state.m_vAnalog1Dir.z = iAnalog1Z/65536.0f;
    pStick->state.m_vAnalog1Dir.z = joyState.rglSlider[0]/65536.0f;

	  pStick->state.m_vAnalog2Dir.x = iAnalog2X/32768.0f;
	  pStick->state.m_vAnalog2Dir.y = iAnalog2Y/32768.0f;
	  //pStick->state.m_vAnalog2Dir.z = iAnalog2Z/65536.0f;
    pStick->state.m_vAnalog2Dir.z = joyState.rglSlider[1]/65536.0f;

    // Clear trigger button bits
    pStick->state.m_buttons&=~((1<<30)|(1<<31));
    // LTrigger:
    if(pStick->state.m_vAnalog1Dir.z>0.30f)  // 30% press => on
      pStick->state.m_buttons|=(1<<31);
    // RTrigger:
    if(pStick->state.m_vAnalog2Dir.z>0.30f)  // 30% press => on
      pStick->state.m_buttons|=(1<<30);

    //--- NickH Debug dump of joy state if button 8 just pressed
    #ifdef DEBUG_BUTTONS
      if( (pStick->state.m_buttons&(1<<7)) && !(pStick->prev.m_buttons&(1<<7)) )
      {
        char acBtns[64],ch;
        int iBtn,iCh;
        unsigned int buttons=pStick->state.m_buttons;
        
        for(iBtn=0,iCh=0;iBtn<32;++iBtn,++iCh)
        {
          ch = (buttons&(1<<iBtn)) ? 'X' : '-';
          acBtns[iCh]=ch;
          if( iBtn%4 == 3)
            acBtns[++iCh]=' ';
        }
        acBtns[iCh]=0;
        m_pLog->Log(" Btn:%s\n",acBtns);
        m_pLog->Log(" Axis: %0.2f %0.2f %0.2f %0.2f %0.2f %0.2f\n",
          pStick->state.m_vAnalog1Dir.x,
          pStick->state.m_vAnalog1Dir.y,
          pStick->state.m_vAnalog1Dir.z,
          pStick->state.m_vAnalog2Dir.x,
          pStick->state.m_vAnalog2Dir.y,
          pStick->state.m_vAnalog2Dir.z);
        m_pLog->Log(" Dir: %c%c%c%c\n",
          pStick->state.m_dirs[JOY_DIR_LEFT]   ? 'L' : '-',
          pStick->state.m_dirs[JOY_DIR_RIGHT]  ? 'R' : '-',
          pStick->state.m_dirs[JOY_DIR_UP]     ? 'U' : '-',
          pStick->state.m_dirs[JOY_DIR_DOWN]   ? 'D' : '-'
          );
        m_pLog-> Log(" Hat: %c%c%c%c\n",
          pStick->state.m_hatdirs[JOY_DIR_LEFT]   ? 'L' : '-',
          pStick->state.m_hatdirs[JOY_DIR_RIGHT]  ? 'R' : '-',
          pStick->state.m_hatdirs[JOY_DIR_UP]     ? 'U' : '-',
          pStick->state.m_hatdirs[JOY_DIR_DOWN]   ? 'D' : '-'
          );
      }
    #endif
#endif	
  }
}

///////////////////////////////////////////
int CJoystick::GetDir(int idCtrl)
{
	if (!m_initialized || ((unsigned)idCtrl>=m_numjoysticks) || !m_vSticks[idCtrl]->m_bInit) 
		return (JOY_DIR_NONE);
	
	for (int k=0;k<4;k++) 
		if (m_vSticks[idCtrl]->state.m_dirs[k]) 
			return (k);
		
	return (JOY_DIR_NONE);
}

///////////////////////////////////////////
int CJoystick::GetHatDir(int idCtrl)
{
	if (!m_initialized || ((unsigned)idCtrl>=m_numjoysticks) || !m_vSticks[idCtrl]->m_bInit) 
		return (JOY_DIR_NONE);
	
	for (int k=0;k<4;k++) 
		if (m_vSticks[idCtrl]->state.m_hatdirs[k]) 
			return (k);
		
	return (JOY_DIR_NONE);
}

///////////////////////////////////////////
int CJoystick::GetDirPressed(int idCtrl)
{
  if (!m_initialized || ((unsigned)idCtrl>=m_numjoysticks) || !m_vSticks[idCtrl]->m_bInit) 
    return (JOY_DIR_NONE);

  for (int k=0;k<4;k++) 
    if (m_vSticks[idCtrl]->state.m_dirs[k] && !m_vSticks[idCtrl]->prev.m_dirs[k]) 
      return (k);

  return (JOY_DIR_NONE);
}

///////////////////////////////////////////
int CJoystick::GetDirReleased(int idCtrl)
{
  if (!m_initialized || ((unsigned)idCtrl>=m_numjoysticks) || !m_vSticks[idCtrl]->m_bInit) 
    return (JOY_DIR_NONE);

  for (int k=0;k<4;k++) 
    if (!m_vSticks[idCtrl]->state.m_dirs[k] && m_vSticks[idCtrl]->prev.m_dirs[k]) 
      return (k);

  return (JOY_DIR_NONE);
}

///////////////////////////////////////////
int CJoystick::GetHatDirPressed(int idCtrl)
{
  if (!m_initialized || ((unsigned)idCtrl>=m_numjoysticks) || !m_vSticks[idCtrl]->m_bInit) 
    return (JOY_DIR_NONE);

  for (int k=0;k<4;k++) 
    if (m_vSticks[idCtrl]->state.m_hatdirs[k] && !m_vSticks[idCtrl]->prev.m_hatdirs[k]) 
      return (k);

  return (JOY_DIR_NONE);
}

///////////////////////////////////////////
int CJoystick::GetHatDirReleased(int idCtrl)
{
  if (!m_initialized || ((unsigned)idCtrl>=m_numjoysticks) || !m_vSticks[idCtrl]->m_bInit) 
    return (JOY_DIR_NONE);

  for (int k=0;k<4;k++) 
    if (!m_vSticks[idCtrl]->state.m_hatdirs[k] && m_vSticks[idCtrl]->prev.m_hatdirs[k]) 
      return (k);

  return (JOY_DIR_NONE);
}

///////////////////////////////////////////
Vec3 CJoystick::GetAnalog1Dir(int idCtrl) const
{
	if ((unsigned)idCtrl < m_numjoysticks)
		return m_vSticks[idCtrl]->state.m_vAnalog1Dir;
	else
		return Vec3(0,0,0);
}
///////////////////////////////////////////
Vec3 CJoystick::GetAnalog2Dir(int idCtrl) const
{
	if ((unsigned)idCtrl < m_numjoysticks)
		return m_vSticks[idCtrl]->state.m_vAnalog2Dir;
	else
		return Vec3(0,0,0);
}

float CJoystick::GetHGain(int idCtrl)
{
  float fRet=1.0f;
  if ((unsigned)idCtrl < m_numjoysticks)
  {
    fRet=m_vSticks[idCtrl]->m_fHGain;
  }
  return fRet;
}
float CJoystick::GetHScale(int idCtrl)
{
  float fRet=1.0f;
  if ((unsigned)idCtrl < m_numjoysticks)
  {
    fRet=m_vSticks[idCtrl]->m_fHScale;
  }
  return fRet;
}
float CJoystick::GetVGain(int idCtrl)
{
  float fRet=1.0f;
  if ((unsigned)idCtrl < m_numjoysticks)
  {
    fRet=m_vSticks[idCtrl]->m_fVGain;
  }
  return fRet;
}
float CJoystick::GetVScale(int idCtrl)
{
  float fRet=1.0f;
  if ((unsigned)idCtrl < m_numjoysticks)
  {
    fRet=m_vSticks[idCtrl]->m_fVScale;
  }
  return fRet;
}
void CJoystick::SetHGain(int idCtrl,float fHGain)
{
  if ((unsigned)idCtrl < m_numjoysticks)
  {
    m_vSticks[idCtrl]->m_fHGain=fHGain;
  }
}
void CJoystick::SetHScale(int idCtrl,float fHScale)
{
  if ((unsigned)idCtrl < m_numjoysticks)
  {
    m_vSticks[idCtrl]->m_fHScale=fHScale;
  }
}
void CJoystick::SetVGain(int idCtrl,float fVGain)
{
  if ((unsigned)idCtrl < m_numjoysticks)
  {
    m_vSticks[idCtrl]->m_fVGain=fVGain;
  }
}
void CJoystick::SetVScale(int idCtrl,float fVScale)
{
  if ((unsigned)idCtrl < m_numjoysticks)
  {
    m_vSticks[idCtrl]->m_fVScale=fVScale;
  }
}


///////////////////////////////////////////
int CJoystick::GetNumButtons(int idCtrl)
{
  int iRet=0;

  if ((unsigned)idCtrl < m_numjoysticks)
    iRet=m_vSticks[idCtrl]->m_numbuttons;
	
  return iRet;
}

///////////////////////////////////////////
bool CJoystick::IsButtonPressed(int idCtrl,int buttonnum)
{
  bool bRet=false;

	if (m_initialized && ((unsigned)idCtrl<m_numjoysticks) && ((unsigned)buttonnum<MAX_BUTTON) )
  {
	  bRet=!!(m_vSticks[idCtrl]->state.m_buttons & (1<<buttonnum));
  }

	return bRet;
}

bool CJoystick::IsBtnDown(int idCtrl,int idButton)
{
  bool bRet=false;

  if (m_initialized && ((unsigned)idCtrl<m_numjoysticks) && ((unsigned)idButton<MAX_BUTTON) )
  {
    bRet=!!(m_vSticks[idCtrl]->state.m_buttons & (1<<idButton));
  }

  return bRet;
}

bool CJoystick::IsBtnPressed(int idCtrl,int idButton)
{
  bool bRet=false;

  if (m_initialized && ((unsigned)idCtrl<m_numjoysticks) && ((unsigned)idButton<MAX_BUTTON) )
  {
    bRet=!!(m_vSticks[idCtrl]->state.m_buttons & ~m_vSticks[idCtrl]->prev.m_buttons & (1<<idButton));
  }

  return bRet;
}

bool CJoystick::IsBtnReleased(int idCtrl,int idButton)
{
  bool bRet=false;

  if (m_initialized && ((unsigned)idCtrl<m_numjoysticks) && ((unsigned)idButton<MAX_BUTTON) )
  {
    bRet=!!(~m_vSticks[idCtrl]->state.m_buttons & m_vSticks[idCtrl]->prev.m_buttons & (1<<idButton));
  }

  return bRet;
}

void CJoystick::ClearState()
{
  if (m_initialized)
  {
    VSticksIter itStick,itEnd;

    for(itStick=m_vSticks.begin(),itEnd=m_vSticks.end();itStick!=itEnd;++itStick)
    {
      if((*itStick)->m_bInit)
        (*itStick)->ClearAllState();
    }
  }
}

///////////////////////////////////////////
void CJoystick::ShutDown()
{
	m_pLog->LogToFile("Joystick Shutdown\n");
	m_initialized=false;
}

