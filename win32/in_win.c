/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#include "../client/client.h"
#include "winquake.h"

extern unsigned sys_msg_time;

// Joystick defines and variables
#define JOY_ABSOLUTE_AXIS	0	// control like a joystick
#define JOY_RELATIVE_AXIS	16	// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6	// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList
{
	AxisNada = 0, 
	AxisForward, 
	AxisLook, 
	AxisSide, 
	AxisTurn, 
	AxisUp
};

DWORD dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX,
	JOY_RETURNY,
	JOY_RETURNZ,
	JOY_RETURNR,
	JOY_RETURNU,
	JOY_RETURNV
};

static DWORD dwAxisMap[JOY_MAX_AXES];
static DWORD dwControlMap[JOY_MAX_AXES];
static PDWORD pdwRawValue[JOY_MAX_AXES];

cvar_t *m_noaccel; //sul
cvar_t *in_mouse;
cvar_t *in_joystick;

cvar_t *autosensitivity;

// None of these cvars are saved over a session.
// This means that advanced controller configuration needs to be executed each time.
// This avoids any problems with getting back to a default usage or when changing 
// from one controller to another. This way at least something works.
cvar_t *joy_name;
cvar_t *joy_advanced;
cvar_t *joy_advaxisx;
cvar_t *joy_advaxisy;
cvar_t *joy_advaxisz;
cvar_t *joy_advaxisr;
cvar_t *joy_advaxisu;
cvar_t *joy_advaxisv;
cvar_t *joy_forwardthreshold;
cvar_t *joy_sidethreshold;
cvar_t *joy_pitchthreshold;
cvar_t *joy_yawthreshold;
cvar_t *joy_forwardsensitivity;
cvar_t *joy_sidesensitivity;
cvar_t *joy_pitchsensitivity;
cvar_t *joy_yawsensitivity;
cvar_t *joy_upthreshold;
cvar_t *joy_upsensitivity;

static qboolean joy_avail;
static qboolean joy_advancedinit;
static qboolean joy_haspov;
static DWORD joy_oldbuttonstate;
static DWORD joy_oldpovstate;

static int joy_id;
static DWORD joy_flags;
static DWORD joy_numbuttons;

static JOYINFOEX ji;

extern cursor_t cursor;

static qboolean in_appactive;

#pragma region ======================= Mouse control

// Mouse variables
cvar_t *m_filter;
static qboolean mlooking;

static int mouse_buttons;
static int mouse_oldbuttonstate;
static int mouse_x;
static int mouse_y;

static qboolean	mouseactive; // False when not focus app

static qboolean	restore_spi;
static qboolean	mouseinitialized;
static int originalmouseparms[3];
static int newmouseparms[3] = { 0, 0, 1 };
static qboolean	mouseparmsvalid;

static int window_center_x;
static int window_center_y;

static void IN_MLookDown(void)
{
	mlooking = true;
}

static void IN_MLookUp(void)
{
	mlooking = false;
	if (!freelook->integer && lookspring->integer)
		IN_CenterView();
}

// Called when the window gains focus or changes in some way
static void IN_ActivateMouse(void)
{
	if (!mouseinitialized)
		return;

	if (!in_mouse->value)
	{
		mouseactive = false;
		return;
	}

	if (mouseactive)
		return;

	mouseactive = true;

	if (m_noaccel->integer) 
		newmouseparms[2] = 0; //sul XP fix?
	else 
		newmouseparms[2] = 1;

	if (mouseparmsvalid)
		restore_spi = SystemParametersInfo(SPI_SETMOUSE, 0, newmouseparms, 0);

	const int width = GetSystemMetrics(SM_CXSCREEN);
	const int height = GetSystemMetrics(SM_CYSCREEN);

	RECT window_rect;
	GetWindowRect(cl_hwnd, &window_rect);
	window_rect.left = max(window_rect.left, 0);
	window_rect.top = max(window_rect.top, 0);
	window_rect.right = min(window_rect.right, width - 1);
	window_rect.bottom = min(window_rect.bottom, height - 1);

	window_center_x = (window_rect.right + window_rect.left) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	SetCursorPos(window_center_x, window_center_y);

	SetCapture(cl_hwnd);
	ClipCursor(&window_rect);
	while (ShowCursor(FALSE) > -1)
		;
}

// Called when the window loses focus
static void IN_DeactivateMouse(void)
{
	if (!mouseinitialized || !mouseactive)
		return;

	if (restore_spi)
		SystemParametersInfo(SPI_SETMOUSE, 0, originalmouseparms, 0);

	mouseactive = false;

	ClipCursor(NULL);
	ReleaseCapture();
	while (ShowCursor(TRUE) < 0)
		;
}

extern void UI_RefreshCursorMenu(void);
extern void UI_RefreshCursorLink(void);

static void IN_StartupMouse(void)
{
	cvar_t *cv = Cvar_Get("in_initmouse", "1", CVAR_NOSET);
	if (!cv->integer) 
		return; 

	// Knightmare- added Psychospaz's menu mouse support
	UI_RefreshCursorMenu();
	UI_RefreshCursorLink();

	//mxd. Center cursor in screen
	cursor.x = viddef.width / 2;
	cursor.y = viddef.height / 2;

	cursor.mouseaction = false;

	mouseinitialized = true;
	mouseparmsvalid = SystemParametersInfo(SPI_GETMOUSE, 0, originalmouseparms, 0);
	mouse_buttons = 5; // was 3
}

void IN_MouseEvent(int mstate)
{
	if (!mouseinitialized)
		return;

	// Set menu cursor buttons
	if (cls.key_dest == key_menu)
	{
		const int multiclicktime = 750;
		const int max = min(mouse_buttons, MENU_CURSOR_BUTTON_MAX);

		for (int i = 0; i < max; i++)
		{
			if ((mstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i)))
			{
				// Mouse press down
				if (sys_msg_time - cursor.buttontime[i] < multiclicktime)
					cursor.buttonclicks[i] += 1;
				else
					cursor.buttonclicks[i] = 1;

				if (cursor.buttonclicks[i] > max)
					cursor.buttonclicks[i] = max;

				cursor.buttontime[i] = sys_msg_time;

				cursor.buttondown[i] = true;
				cursor.buttonused[i] = false;
				cursor.mouseaction = true;
			}
			else if (!(mstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i)))
			{
				// Mouse let go
				cursor.buttondown[i] = false;
				cursor.buttonused[i] = false;
				cursor.mouseaction = true;
			}
		}
	}	

	// Perform button actions
	//mxd. Moved below "set menu cursor buttons" block, because if Key_Event changes cls.key_dest to key_menu, the above block will immediately trigger mouse click action,
	// which will select "Options" menu after clicking during ID logo cinematic, because it happens to be at the center of the screen
	for (int i = 0; i < mouse_buttons; i++)
	{
		if ((mstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i)))
			Key_Event(K_MOUSE1 + i, true, sys_msg_time);
		else if (!(mstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i)))
			Key_Event(K_MOUSE1 + i, false, sys_msg_time);
	}

	mouse_oldbuttonstate = mstate;
}

static void IN_MouseMove(usercmd_t *cmd)
{
	static int old_mouse_x = 0; //mxd. Made local
	static int old_mouse_y = 0;
	
	if (!autosensitivity)
		autosensitivity = Cvar_Get("autosensitivity", "1", CVAR_ARCHIVE);

	if (!mouseactive)
		return;

	// Find mouse movement
	POINT current_pos;
	if (!GetCursorPos(&current_pos))
		return;

	const int mx = current_pos.x - window_center_x;
	const int my = current_pos.y - window_center_y;

	if (m_filter->value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5f;
		mouse_y = (my + old_mouse_y) * 0.5f;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	// Now to set the menu cursor
	if (cls.key_dest == key_menu)
	{
		cursor.oldx = cursor.x;
		cursor.oldy = cursor.y;

		cursor.x += mouse_x * menu_sensitivity->value;
		cursor.y += mouse_y * menu_sensitivity->value;

		if (cursor.x != cursor.oldx || cursor.y != cursor.oldy)
			cursor.mouseaction = true;

		cursor.x = clamp(cursor.x, 0, viddef.width);
		cursor.y = clamp(cursor.y, 0, viddef.height);
	}
	else
	{
		cursor.oldx = 0;
		cursor.oldy = 0;

		//psychospaz - zooming in preserves sensitivity
		if (autosensitivity->value && cl.base_fov < 90)
		{
			mouse_x *= sensitivity->value * (cl.base_fov / 90.0f);
			mouse_y *= sensitivity->value * (cl.base_fov / 90.0f);
		}
		else
		{
			mouse_x *= sensitivity->value;
			mouse_y *= sensitivity->value;
		}

		// Add mouse X/Y movement to cmd
		if ((in_strafe.state & KEYSTATE_DOWN) || (lookstrafe->value && mlooking))
			cmd->sidemove += m_side->value * mouse_x;
		else
			cl.viewangles[YAW] -= m_yaw->value * mouse_x;

		if ((mlooking || freelook->value) && !(in_strafe.state & KEYSTATE_DOWN))
			cl.viewangles[PITCH] += m_pitch->value * mouse_y;
		else
			cmd->forwardmove -= m_forward->value * mouse_y;
	}

	// Force the mouse to the center, so there's room to move
	if (mx || my)
		SetCursorPos(window_center_x, window_center_y);
}

#pragma endregion

#pragma region ======================= Joystick control

static void IN_StartupJoystick(void)
{
	JOYCAPS jc;
	MMRESULT mmr = JOYERR_UNPLUGGED;

	// Assume no joystick
	joy_avail = false;

	// Abort startup if user requests no joystick
	cvar_t *cv = Cvar_Get("in_initjoy", "1", CVAR_NOSET);
	if (!cv->integer)
		return;

	// Verify joystick driver is present
	const int numdevs = joyGetNumDevs();
	if (numdevs == 0)
		return;

	// cycle through the joystick ids for the first valid one
	for (joy_id = 0; joy_id < numdevs; joy_id++)
	{
		memset(&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		mmr = joyGetPosEx(joy_id, &ji);
		if (mmr == JOYERR_NOERROR)
			break;
	}

	// Abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Com_Printf("\njoystick not found -- no valid joysticks (%x)\n\n", mmr);
		return;
	}

	// Get the capabilities of the selected joystick. Abort startup if command fails
	memset(&jc, 0, sizeof(jc));
	mmr = joyGetDevCaps(joy_id, &jc, sizeof(jc));
	if (mmr != JOYERR_NOERROR)
	{
		Com_Printf("\njoystick not found -- invalid joystick capabilities (%x)\n\n", mmr);
		return;
	}

	// Save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// Old button and POV states default to no buttons pressed
	joy_oldbuttonstate = 0;
	joy_oldpovstate = 0;

	// Mark the joystick as available and advanced initialization not completed.
	// This is needed as cvars are not available during initialization.
	joy_avail = true;
	joy_advancedinit = false;

	Com_Printf("\njoystick detected\n\n");
}

static PDWORD RawValuePointer(int axis)
{
	switch (axis)
	{
		case JOY_AXIS_X: return &ji.dwXpos;
		case JOY_AXIS_Y: return &ji.dwYpos;
		case JOY_AXIS_Z: return &ji.dwZpos;
		case JOY_AXIS_R: return &ji.dwRpos;
		case JOY_AXIS_U: return &ji.dwUpos;
		case JOY_AXIS_V: return &ji.dwVpos;

		default: // We need a default return
			return &ji.dwXpos;
	}
}

static void Joy_AdvancedUpdate_f(void)
{
	// Called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available

	// Initialize all the maps
	for (int i = 0; i < JOY_MAX_AXES; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if (joy_advanced->integer == 0)
	{
		// Default joystick initialization. 2 axes only with joystick control.
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		Com_Printf("\n%s configured\n\n", joy_name->string); // Notify user of advanced controller

		// Advanced initialization here
		// Data supplied by user via joy_axisn cvars
		DWORD dwTemp = (DWORD)joy_advaxisx->value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;

		dwTemp = (DWORD)joy_advaxisy->value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;

		dwTemp = (DWORD)joy_advaxisz->value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;

		dwTemp = (DWORD)joy_advaxisr->value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;

		dwTemp = (DWORD)joy_advaxisu->value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;

		dwTemp = (DWORD)joy_advaxisv->value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// Compute the axes to collect from DirectInput
	joy_flags = (JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV);
	for (int i = 0; i < JOY_MAX_AXES; i++)
		if (dwAxisMap[i] != AxisNada)
			joy_flags |= dwAxisFlags[i];
}

void IN_Commands(void)
{
	if (!joy_avail)
		return;

	// Loop through the joystick buttons.
	// Key a joystick event or auxillary event for higher number buttons for each state change.
	const DWORD buttonstate = ji.dwButtons;
	for (int i = 0; i < (int)joy_numbuttons; i++)
	{
		if ((buttonstate & (1 << i)) && !(joy_oldbuttonstate & (1 << i)))
		{
			const int key_index = (i < 4 ? K_JOY1 : K_AUX1);
			Key_Event(key_index + i, true, 0);
		}

		if (!(buttonstate & (1 << i)) && (joy_oldbuttonstate & (1 << i)))
		{
			const int key_index = (i < 4 ? K_JOY1 : K_AUX1);
			Key_Event(key_index + i, false, 0);
		}
	}

	joy_oldbuttonstate = buttonstate;

	if (joy_haspov)
	{
		// Convert POV information into 4 bits of state information.
		// This avoids any potential problems related to moving from one
		// direction to another without going through the center position.
		DWORD povstate = 0;
		if (ji.dwPOV != JOY_POVCENTERED)
		{
			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}

		// Determine which bits have changed and key an auxillary event for each change
		for (int i = 0; i < 4; i++)
		{
			if ((povstate & (1 << i)) && !(joy_oldpovstate & (1 << i)))
				Key_Event(K_AUX29 + i, true, 0);

			if (!(povstate & (1 << i)) && (joy_oldpovstate & (1 << i)))
				Key_Event(K_AUX29 + i, false, 0);
		}

		joy_oldpovstate = povstate;
	}
}

static qboolean IN_ReadJoystick(void)
{
	memset(&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	return (joyGetPosEx(joy_id, &ji) == JOYERR_NOERROR);
}

static void IN_JoyMove(usercmd_t *cmd)
{
	// Complete initialization if first time in.
	// This is needed as cvars are not available at initialization time.
	if (!joy_advancedinit)
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = true;
	}

	// Verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick->value)
		return;

	// Collect the joystick data, if possible
	if (!IN_ReadJoystick())
		return;

	float speed;
	if ((in_speed.state & KEYSTATE_DOWN) ^ cl_run->integer)
		speed = 2;
	else
		speed = 1;

	const float aspeed = speed * cls.netFrameTime;

	// Loop through the axes
	for (int i = 0; i < JOY_MAX_AXES; i++)
	{
		// Get the floating point zero-centered, potentially-inverted data for the current axis
		float fAxisValue = (float)*pdwRawValue[i];

		// Move centerpoint to zero
		fAxisValue -= 32768.0f;

		// Convert range from -32768..32767 to -1..1 
		fAxisValue /= 32768.0f;

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if (!joy_advanced->integer && mlooking)
			{
				// User wants forward control to become look control.
				if (fabsf(fAxisValue) > joy_pitchthreshold->value)
				{
					// If mouse invert is on, invert the joystick pitch value.
					// Only absolute control support here (joy_advanced is false).
					if (m_pitch->value < 0.0)
					{
						if (autosensitivity->value && cl.base_fov < 90) // Knightmare added
							cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity->value * (cl.base_fov / 90.0f)) * aspeed * cl_pitchspeed->value;
						else
							cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						if (autosensitivity->value && cl.base_fov < 90) // Knightmare added
							cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value * (cl.base_fov / 90.0f)) * aspeed * cl_pitchspeed->value;
						else
							cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
				}
			}
			else
			{
				// User wants forward control to be forward control
				if (fabsf(fAxisValue) > joy_forwardthreshold->value)
					cmd->forwardmove += (fAxisValue * joy_forwardsensitivity->value) * speed * cl_forwardspeed->value;
			}
			break;

		case AxisSide:
			if (fabsf(fAxisValue) > joy_sidethreshold->value)
				cmd->sidemove += (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
			break;

		case AxisUp:
			if (fabsf(fAxisValue) > joy_upthreshold->value)
				cmd->upmove += (fAxisValue * joy_upsensitivity->value) * speed * cl_upspeed->value;
			break;

		case AxisTurn:
			if ((in_strafe.state & KEYSTATE_DOWN) || (lookstrafe->value && mlooking))
			{
				// User wants turn control to become side control
				if (fabsf(fAxisValue) > joy_sidethreshold->value)
					cmd->sidemove -= (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
			}
			else
			{
				// User wants turn control to be turn control
				if (fabsf(fAxisValue) > joy_yawthreshold->value)
				{
					if (dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						if (autosensitivity->value && cl.base_fov < 90) // Knightmare added
							cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value * (cl.base_fov / 90.0f)) * aspeed * cl_yawspeed->value;
						else
							cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * aspeed * cl_yawspeed->value;
					}
					else
					{
						if (autosensitivity->value && cl.base_fov < 90) // Knightmare added
							cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value * (cl.base_fov / 90.0f)) * speed * 180.0f;
						else
							cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * speed * 180.0f;
					}

				}
			}
			break;

		case AxisLook:
			if (mlooking)
			{
				if (fabsf(fAxisValue) > joy_pitchthreshold->value)
				{
					// Pitch movement detected and pitch movement desired by user
					if (dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						if (autosensitivity->value && cl.base_fov < 90) // Knightmare added
							cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value * (cl.base_fov / 90.0f)) * aspeed * cl_pitchspeed->value;
						else
							cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						if (autosensitivity->value && cl.base_fov < 90) // Knightmare added
							cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value * (cl.base_fov / 90.0f)) * speed * 180.0f;
						else
							cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * speed * 180.0f;
					}
				}
			}
			break;

		default:
			break;
		}
	}
}

#pragma endregion

#pragma region ======================= Input processing

cvar_t *v_centermove;
cvar_t *v_centerspeed;

void IN_Init(void)
{
	// Mouse variables
	autosensitivity	= Cvar_Get("autosensitivity",	"1", CVAR_ARCHIVE);
	m_noaccel		= Cvar_Get("m_noaccel",			"0", CVAR_ARCHIVE); //sul  enables mouse acceleration XP fix?
	m_filter		= Cvar_Get("m_filter",			"0", 0);
	in_mouse		= Cvar_Get("in_mouse",			"1", CVAR_ARCHIVE);

	// Joystick variables
	in_joystick				= Cvar_Get("in_joystick",				"0",		CVAR_ARCHIVE);
	joy_name				= Cvar_Get("joy_name",					"joystick",	0);
	joy_advanced			= Cvar_Get("joy_advanced",				"0",		0);
	joy_advaxisx			= Cvar_Get("joy_advaxisx",				"0",		0);
	joy_advaxisy			= Cvar_Get("joy_advaxisy",				"0",		0);
	joy_advaxisz			= Cvar_Get("joy_advaxisz",				"0",		0);
	joy_advaxisr			= Cvar_Get("joy_advaxisr",				"0",		0);
	joy_advaxisu			= Cvar_Get("joy_advaxisu",				"0",		0);
	joy_advaxisv			= Cvar_Get("joy_advaxisv",				"0",		0);
	joy_forwardthreshold	= Cvar_Get("joy_forwardthreshold",		"0.15",		0);
	joy_sidethreshold		= Cvar_Get("joy_sidethreshold",			"0.15",		0);
	joy_upthreshold  		= Cvar_Get("joy_upthreshold",			"0.15",		0);
	joy_pitchthreshold		= Cvar_Get("joy_pitchthreshold",		"0.15",		0);
	joy_yawthreshold		= Cvar_Get("joy_yawthreshold",			"0.15",		0);
	joy_forwardsensitivity	= Cvar_Get("joy_forwardsensitivity",	"-1",		0);
	joy_sidesensitivity		= Cvar_Get("joy_sidesensitivity",		"-1",		0);
	joy_upsensitivity		= Cvar_Get("joy_upsensitivity",			"-1",		0);
	joy_pitchsensitivity	= Cvar_Get("joy_pitchsensitivity",		"1",		0);
	joy_yawsensitivity		= Cvar_Get("joy_yawsensitivity",		"-1",		0);

	// Centering
	v_centermove	= Cvar_Get("v_centermove", "0.15", 0);
	v_centerspeed	= Cvar_Get("v_centerspeed", "500", 0);

	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);

	Cmd_AddCommand("joy_advancedupdate", Joy_AdvancedUpdate_f);

	IN_StartupMouse();
	IN_StartupJoystick();
}

void IN_Shutdown(void)
{
	IN_DeactivateMouse();
}

// Called when the main window gains or loses focus.
// The window may have been destroyed and recreated between a deactivate and an activate.
void IN_Activate(qboolean active)
{
	in_appactive = active;
	mouseactive = !active; // Force a new window check or turn off
}

// Called every frame, even if not generating commands
void IN_Frame(void)
{
	if (!mouseinitialized)
		return;

	if (!in_mouse || !in_appactive)
	{
		IN_DeactivateMouse();
		return;
	}

	//Knightmare- added Psychospaz's mouse menu support
	if ((!cl.refresh_prepped && cls.key_dest != key_menu) || cls.consoleActive) // mouse used in menus...
	{
		// Temporarily deactivate if in fullscreen
		if (Cvar_VariableValue("vid_fullscreen") == 0)
		{
			IN_DeactivateMouse();
			return;
		}
	}

	IN_ActivateMouse();
}

extern void UI_Think_MouseCursor(void);

void IN_Move(usercmd_t *cmd)
{
	if (!ActiveApp)
		return;

	IN_MouseMove(cmd);

	// Knightmare- added Psychospaz's mouse support
	if (cls.key_dest == key_menu && !cls.consoleActive) // Knightmare added
		UI_Think_MouseCursor();

	IN_JoyMove(cmd);
}

static void IN_ClearStates(void)
{
	mouse_oldbuttonstate = 0;
}

#pragma endregion