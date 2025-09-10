
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Joystick.h
//
//	History:
//	-Jan 31,2001:Created by Marco Corbetta
//  -March 2005: NickH: Refactored and added Pressed,Down,Released functionality.
//////////////////////////////////////////////////////////////////////

#ifndef JOYSTICK_H
#define JOYSTICK_H

#define JOY_DIR_UP		0
#define JOY_DIR_DOWN	1
#define JOY_DIR_LEFT	2
#define JOY_DIR_RIGHT	3
#define JOY_DIR_NONE	-1

#define JOY_UPDATE_TIME		1.0f/10.0f //10 times for second


#include "Cry_Math.h"

#include <vector>


/*
===========================================
The Joystick interface Class
===========================================
*/
struct ILog;
//////////////////////////////////////////////////////////////////////

struct StickState
{
  uint32 m_buttons;  // spec allows a max of 32 buttons
  unsigned char m_dirs[4];
  unsigned char m_hatdirs[4];	

  // this is for returning analog controller directional input. Input is in the range -1..1 for the x and y axis. z unused
  Vec3	m_vAnalog1Dir;  // Axis 1..3 (commonly referred to as xyz)
  // this is for returning analog controller directional input. Input is in the range -1..1 for the x and y axis. z unused
  Vec3	m_vAnalog2Dir;  // Axis 4..6 (commonly referred to as ruv)

  
  StickState() { ClearState(); }
  void ClearState();
  StickState & operator=(const StickState &state);
};

struct Stick
{
  bool	m_hatswitch;
  int		m_numbuttons;

  #ifdef WIN32
    LPDIRECTINPUTDEVICE8 m_pDIStick;
  #else
    unsigned m_idStick;
  #endif

  bool m_bInit;
  float m_fHGain;
  float m_fHScale;
  float m_fVGain;
  float m_fVScale;
  StickState state;
  StickState prev;

  Stick() : m_hatswitch(false),m_numbuttons(0),m_bInit(false)
            //,m_fHGain(2.4f),m_fHScale(1.0f),m_fVGain(2.4f),m_fVScale(0.4f)
						,m_fHGain(1.0f),m_fHScale(1.0f),m_fVGain(1.0f),m_fVScale(1.0f)
    {
      #ifdef WIN32
        m_pDIStick=0;
      #else
        m_idStick=0;
      #endif
    }

  void ClearAllState() { state.ClearState(); prev.ClearState(); }
  void ClearCurrentState() { state.ClearState(); }
  void SaveState() { prev=state; }
};

typedef std::vector<Stick *> VSticks;
typedef std::vector<Stick *>::iterator VSticksIter;

class CJoystick
{
public:
	CJoystick();
	~CJoystick();

  #ifdef WIN32
	  bool	Init(LPDIRECTINPUT8 lpdi,HINSTANCE hInst,HWND hWnd,ILog *pLog);
  #else
    bool	Init(ILog *pLog);
  #endif
	void	Update();			
	void	ShutDown();

	int		GetNumButtons(int idCtrl);	
	bool	IsButtonPressed(int idCtrl,int buttonnum);
  
	int		GetDir(int idCtrl);
	int		GetHatDir(int idCtrl);

  int		GetDirPressed(int idCtrl);
  int		GetDirReleased(int idCtrl);
  int		GetHatDirPressed(int idCtrl);
  int		GetHatDirReleased(int idCtrl);

  bool IsBtnDown(int idCtrl,int idButton);
  bool IsBtnPressed(int idCtrl,int idButton);
  bool IsBtnReleased(int idCtrl,int idButton);

	// get analog direction (two analog sticks)
	Vec3	GetAnalog1Dir(int idCtrl) const;
	Vec3	GetAnalog2Dir(int idCtrl) const;

  // Get/Set Gain and scale used for joy sensitivity
  float GetHGain(int idCtrl);
  float GetHScale(int idCtrl);
  float GetVGain(int idCtrl);
  float GetVScale(int idCtrl);
  void SetHGain(int idCtrl,float fHGain);
  void SetHScale(int idCtrl,float fHScale);
  void SetVGain(int idCtrl,float fVGain);
  void SetVScale(int idCtrl,float fVScale);

  void ClearState();

private:	

	bool	m_initialized;
	bool	m_hatswitch;

  unsigned int	m_numjoysticks;	//!<	Number of available joysticks
  
  //Stick **m_apSticks;             //!< State of each stick.
  VSticks m_vSticks;                //!< State of each stick.


	//int		m_numbuttons;	
	float	m_joytime;
	
	//unsigned char m_buttons[8];
	//unsigned char m_dirs[4];
	//unsigned char m_hatdirs[4];	

	// this is for returning analog controller directional input. Input is in the range -1..1 for the x and y axis. z unused
	//Vec3	*m_vAnalog1Dir;
	//Vec3	*m_vAnalog2Dir;

	ILog *m_pLog;

  #ifdef WIN32
    LPDIRECTINPUT8 m_pDI;
    HINSTANCE m_hInst;
    HWND m_hWnd;

    // Internal Direct input init and enumeration of joysticks
    bool InitJoyDI();
    void AddJoyDI(LPCDIDEVICEINSTANCE lpddi);
    bool PollDI(Stick *pStick);

    // Direct Input joystick enumeration callback
    friend BOOL CALLBACK Joy_DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi,LPVOID pvRef);
  #endif
};

#endif
