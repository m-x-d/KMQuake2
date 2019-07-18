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
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

cvar_t *cl_nodelta;

static unsigned frame_msec;
static unsigned old_sys_frame_time;

#pragma region ======================= KEY BUTTONS

/*
Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


Key_Event (int key, qboolean down, unsigned time);

  +mlook src time
*/

kbutton_t in_klook, in_strafe, in_speed;
static kbutton_t in_left, in_right, in_forward, in_back;
static kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
static kbutton_t in_use, in_attack, in_attack2;
static kbutton_t in_up, in_down;

static int in_impulse;

static void KeyDown(kbutton_t *b)
{
	int k;

	char *c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1; // Typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return; // Repeating key
	
	if (!b->down[0])
	{
		b->down[0] = k;
	}
	else if (!b->down[1])
	{
		b->down[1] = k;
	}
	else
	{
		Com_Printf("Three keys down for a button!\n");
		return;
	}
	
	if (b->state & KEYSTATE_DOWN)
		return; // Still down

	// Save timestamp
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = sys_frame_time - 100;

	b->state |= (KEYSTATE_DOWN | KEYSTATE_IMPULSE_DOWN); // down + impulse down
}

static void KeyUp(kbutton_t *b)
{
	int k;

	char *c = Cmd_Argv(1);
	if (c[0])
	{
		k = atoi(c);
	}
	else
	{
		// Typed manually at the console, assume for unsticking, so clear all
		b->down[0] = 0;
		b->down[1] = 0;
		b->state = KEYSTATE_IMPULSE_UP; // Impulse up

		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return; // Key up without coresponding down (menu pass through)

	if (b->down[0] || b->down[1])
		return;	// Some other key is still holding it down

	if (!(b->state & KEYSTATE_DOWN))
		return; // Still up (this should not happen)

	// Save timestamp
	c = Cmd_Argv(2);
	const unsigned uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	b->state &= ~KEYSTATE_DOWN; // Now up
	b->state |= KEYSTATE_IMPULSE_UP; // Impulse up
}

static void IN_KLookDown() { KeyDown(&in_klook); }
static void IN_KLookUp() { KeyUp(&in_klook); }
static void IN_UpDown() { KeyDown(&in_up); }
static void IN_UpUp() { KeyUp(&in_up); }
static void IN_DownDown() { KeyDown(&in_down); }
static void IN_DownUp() { KeyUp(&in_down); }
static void IN_LeftDown() { KeyDown(&in_left); }
static void IN_LeftUp() { KeyUp(&in_left); }
static void IN_RightDown() { KeyDown(&in_right); }
static void IN_RightUp() { KeyUp(&in_right); }
static void IN_ForwardDown() { KeyDown(&in_forward); }
static void IN_ForwardUp() { KeyUp(&in_forward); }
static void IN_BackDown() { KeyDown(&in_back); }
static void IN_BackUp() { KeyUp(&in_back); }
static void IN_LookupDown() { KeyDown(&in_lookup); }
static void IN_LookupUp() { KeyUp(&in_lookup); }
static void IN_LookdownDown() { KeyDown(&in_lookdown); }
static void IN_LookdownUp() { KeyUp(&in_lookdown); }
static void IN_MoveleftDown() { KeyDown(&in_moveleft); }
static void IN_MoveleftUp() { KeyUp(&in_moveleft); }
static void IN_MoverightDown() { KeyDown(&in_moveright); }
static void IN_MoverightUp() { KeyUp(&in_moveright); }

static void IN_SpeedDown() { KeyDown(&in_speed); }
static void IN_SpeedUp() { KeyUp(&in_speed); }
static void IN_StrafeDown() { KeyDown(&in_strafe); }
static void IN_StrafeUp() { KeyUp(&in_strafe); }

static void IN_AttackDown() { KeyDown(&in_attack); }
static void IN_AttackUp() { KeyUp(&in_attack); }

//Knightmare added
static void IN_Attack2Down() { KeyDown(&in_attack2); }
static void IN_Attack2Up() { KeyUp(&in_attack2); }

static void IN_UseDown() { KeyDown(&in_use); }
static void IN_UseUp() { KeyUp(&in_use); }

static void IN_Impulse() { in_impulse = atoi(Cmd_Argv(1)); }

void IN_CenterView()
{
	cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
}

// Returns the fraction of the frame that the key was down
float CL_KeyState(kbutton_t *key)
{
	key->state &= KEYSTATE_DOWN; // Clear impulses

	int msec = key->msec;
	key->msec = 0;

	if (key->state)
	{
		// Still down
		msec += sys_frame_time - key->downtime;
		key->downtime = sys_frame_time;
	}

	return clamp((float)msec / frame_msec, 0, 1);
}

#pragma endregion 

cvar_t *cl_upspeed;
cvar_t *cl_forwardspeed;
cvar_t *cl_sidespeed;

cvar_t *cl_yawspeed;
cvar_t *cl_pitchspeed;

cvar_t *cl_run;

cvar_t *cl_anglespeedkey;

// Moves the local angle positions
static void CL_AdjustAngles(void)
{
	float speed = cls.netFrameTime;

	if (in_speed.state & KEYSTATE_DOWN)
		speed *= cl_anglespeedkey->value;

	if (!(in_strafe.state & KEYSTATE_DOWN))
	{
		cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState(&in_right);
		cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState(&in_left);
	}

	if (in_klook.state & KEYSTATE_DOWN)
	{
		cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState(&in_forward);
		cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState(&in_back);
	}

	cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState(&in_lookup);
	cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState(&in_lookdown);
}

// Send the intended movement message to the server
void CL_BaseMove(usercmd_t *cmd)
{	
	CL_AdjustAngles();
	
	memset(cmd, 0, sizeof(*cmd));
	
	VectorCopy(cl.viewangles, cmd->angles);
	if (in_strafe.state & KEYSTATE_DOWN)
	{
		cmd->sidemove += cl_sidespeed->value * CL_KeyState(&in_right);
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState(&in_left);
	}

	cmd->sidemove += cl_sidespeed->value * CL_KeyState(&in_moveright);
	cmd->sidemove -= cl_sidespeed->value * CL_KeyState(&in_moveleft);

	cmd->upmove += cl_upspeed->value * CL_KeyState(&in_up);
	cmd->upmove -= cl_upspeed->value * CL_KeyState(&in_down);

	if (!(in_klook.state & KEYSTATE_DOWN))
	{	
		cmd->forwardmove += cl_forwardspeed->value * CL_KeyState(&in_forward);
		cmd->forwardmove -= cl_forwardspeed->value * CL_KeyState(&in_back);
	}	

	// Adjust for speed key / running
	if ((in_speed.state & KEYSTATE_DOWN) ^ cl_run->integer)
	{
		cmd->forwardmove *= 2;
		cmd->sidemove *= 2;
		cmd->upmove *= 2;
	}	
}

static void CL_ClampPitch(void)
{
	float pitch = SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);

	if (pitch > 180)
		pitch -= 360;

	if (cl.viewangles[PITCH] + pitch < -360)
		cl.viewangles[PITCH] += 360; // Wrapped

	if (cl.viewangles[PITCH] + pitch > 360)
		cl.viewangles[PITCH] -= 360; // Wrapped

	if (cl.viewangles[PITCH] + pitch > 89)
		cl.viewangles[PITCH] = 89 - pitch;

	if (cl.viewangles[PITCH] + pitch < -89)
		cl.viewangles[PITCH] = -89 - pitch;
}

static void CL_FinishMove(usercmd_t *cmd)
{
	// Figure button bits
	if (in_attack.state & (KEYSTATE_DOWN | KEYSTATE_IMPULSE_DOWN))
		cmd->buttons |= BUTTON_ATTACK;
	in_attack.state &= ~KEYSTATE_IMPULSE_DOWN;
	
	// Knightmare added
	if (in_attack2.state & (KEYSTATE_DOWN | KEYSTATE_IMPULSE_DOWN))
		cmd->buttons |= BUTTON_ATTACK2;
	in_attack2.state &= ~KEYSTATE_IMPULSE_DOWN;

	if (in_use.state & (KEYSTATE_DOWN | KEYSTATE_IMPULSE_DOWN))
		cmd->buttons |= BUTTON_USE;
	in_use.state &= ~KEYSTATE_IMPULSE_DOWN;

	if (anykeydown && cls.key_dest == key_game)
		cmd->buttons |= BUTTON_ANY;

	// Send milliseconds of time to apply the move
	int ms = cls.netFrameTime * 1000;
	if (ms > 250)
		ms = 100; // Time was unreasonable
	cmd->msec = ms;

	CL_ClampPitch();
	for (int i = 0; i < 3; i++)
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);

	cmd->impulse = in_impulse;
	in_impulse = 0;

	// Send the ambient light level at the player's current position
	cmd->lightlevel = (byte)cl_lightlevel->value;
}

void CL_InitInput()
{
	Cmd_AddCommand("centerview", IN_CenterView);

	Cmd_AddCommand("+moveup", IN_UpDown);
	Cmd_AddCommand("-moveup", IN_UpUp);
	Cmd_AddCommand("+movedown", IN_DownDown);
	Cmd_AddCommand("-movedown", IN_DownUp);
	Cmd_AddCommand("+left", IN_LeftDown);
	Cmd_AddCommand("-left", IN_LeftUp);
	Cmd_AddCommand("+right", IN_RightDown);
	Cmd_AddCommand("-right", IN_RightUp);
	Cmd_AddCommand("+forward", IN_ForwardDown);
	Cmd_AddCommand("-forward", IN_ForwardUp);
	Cmd_AddCommand("+back", IN_BackDown);
	Cmd_AddCommand("-back", IN_BackUp);
	Cmd_AddCommand("+lookup", IN_LookupDown);
	Cmd_AddCommand("-lookup", IN_LookupUp);
	Cmd_AddCommand("+lookdown", IN_LookdownDown);
	Cmd_AddCommand("-lookdown", IN_LookdownUp);
	Cmd_AddCommand("+strafe", IN_StrafeDown);
	Cmd_AddCommand("-strafe", IN_StrafeUp);
	Cmd_AddCommand("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand("+moveright", IN_MoverightDown);
	Cmd_AddCommand("-moveright", IN_MoverightUp);
	Cmd_AddCommand("+speed", IN_SpeedDown);
	Cmd_AddCommand("-speed", IN_SpeedUp);
	Cmd_AddCommand("+attack", IN_AttackDown);
	Cmd_AddCommand("-attack", IN_AttackUp);

	// Knightmare added
	Cmd_AddCommand("+attack2", IN_Attack2Down);
	Cmd_AddCommand("-attack2", IN_Attack2Up);

	Cmd_AddCommand("+use", IN_UseDown);
	Cmd_AddCommand("-use", IN_UseUp);
	Cmd_AddCommand("impulse", IN_Impulse);
	Cmd_AddCommand("+klook", IN_KLookDown);
	Cmd_AddCommand("-klook", IN_KLookUp);

	cl_nodelta = Cvar_Get("cl_nodelta", "0", 0);
}

static usercmd_t CL_CreateCmd(void)
{
	frame_msec = clamp(sys_frame_time - old_sys_frame_time, 1, 200);

	usercmd_t cmd;
	CL_BaseMove(&cmd); // Get basic movement from keyboard
	IN_Move(&cmd); // Allow mice or other external controllers to add to the move
	CL_FinishMove(&cmd);

	old_sys_frame_time = sys_frame_time;

	return cmd;
}

#ifdef CLIENT_SPLIT_NETFRAME

// jec - Adds any new input changes to usercmd that occurred since last Init or RefreshCmd.
void CL_RefreshCmd()
{
	usercmd_t *cmd = &cl.cmds[cls.netchan.outgoing_sequence & (CMD_BACKUP - 1)];

	// Get delta for this sample.
	frame_msec = sys_frame_time - old_sys_frame_time;

	// Bounds checking
	if (frame_msec < 1)
		return;

	frame_msec = min(frame_msec, 200);

	// Get basic movement from keyboard
	CL_BaseMove(cmd);

	// Allow mice or other external controllers to add to the move
	IN_Move(cmd);

	// Update cmd viewangles for CL_PredictMove
	CL_ClampPitch();

	for(int c = 0; c < 3; c++)
		cmd->angles[c] = ANGLE2SHORT(cl.viewangles[c]);

	// Update cmd->msec for CL_PredictMove
	int ms = (int)(cls.netFrameTime * 1000);
	if (ms > 250)
		ms = 100;

	cmd->msec = ms;

	// Update counter
	old_sys_frame_time = sys_frame_time;

	// 7 = starting attack 1  2  4
	// 5 = during attack   1     4 
	// 4 = idle                  4

	// Send packet immediately on important events
	if ((in_attack.state & KEYSTATE_IMPULSE_DOWN) || (in_attack2.state & KEYSTATE_IMPULSE_DOWN) || (in_use.state & KEYSTATE_IMPULSE_DOWN))
		cls.forcePacket = true;
}

// Just updates movement, such as when disconnected.
void CL_RefreshMove()
{	
	usercmd_t *cmd = &cl.cmds[cls.netchan.outgoing_sequence & (CMD_BACKUP - 1)];

	// Get delta for this sample
	frame_msec = sys_frame_time - old_sys_frame_time;

	// bounds checking
	if (frame_msec < 1)
		return;

	frame_msec = min(frame_msec, 200);

	// Get basic movement from keyboard
	CL_BaseMove(cmd);

	// Allow mice or other external controllers to add to the move
	IN_Move(cmd);

	// Update counter
	old_sys_frame_time = sys_frame_time;
}

// jec - Prepares usercmd for transmission, adds all changes that occurred since last Init.
static void CL_FinalizeCmd(void)
{
	usercmd_t *cmd = &cl.cmds[cls.netchan.outgoing_sequence & (CMD_BACKUP - 1)];

	// Set any button hits that occured since last frame
	if (in_attack.state & (KEYSTATE_DOWN | KEYSTATE_IMPULSE_DOWN))
		cmd->buttons |= BUTTON_ATTACK;
	in_attack.state &= ~KEYSTATE_IMPULSE_DOWN;

	// Knightmare added
	if (in_attack2.state & (KEYSTATE_DOWN | KEYSTATE_IMPULSE_DOWN))
		cmd->buttons |= BUTTON_ATTACK2;
	in_attack2.state &= ~KEYSTATE_IMPULSE_DOWN;

	if (in_use.state & (KEYSTATE_DOWN | KEYSTATE_IMPULSE_DOWN))
		cmd->buttons |= BUTTON_USE;
	in_use.state &= ~KEYSTATE_IMPULSE_DOWN;

	if (anykeydown && cls.key_dest == key_game)
		cmd->buttons |= BUTTON_ANY;

	cmd->impulse = in_impulse;
	in_impulse = 0;

	// Set the ambient light level at the player's current position
	cmd->lightlevel = (byte)cl_lightlevel->value;
}

#endif

void CL_SendCmd(qboolean async)
{
	// Clear buffer
	sizebuf_t buf;
	memset(&buf, 0, sizeof(buf));

	// Build a command even if not connected

	// Save this command off for prediction
	int cmdindex = cls.netchan.outgoing_sequence & (CMD_BACKUP - 1);
	cl.cmd_time[cmdindex] = cls.realtime; // For netgraph ping calculation

	if (async) //mxd
		CL_FinalizeCmd();
	else
		cl.cmds[cmdindex] = CL_CreateCmd();

	if (cls.state == ca_disconnected || cls.state == ca_connecting)
		return;

	if (cls.state == ca_connected)
	{
		if (cls.netchan.message.cursize	|| curtime - cls.netchan.last_sent > 1000)
			Netchan_Transmit(&cls.netchan, 0, buf.data);

		return;
	}

	// Send a userinfo update if needed
	if (userinfo_modified)
	{
		CL_FixUpGender();
		userinfo_modified = false;
		MSG_WriteByte(&cls.netchan.message, clc_userinfo);
		MSG_WriteString(&cls.netchan.message, Cvar_Userinfo());
	}

	byte data[128];
	SZ_Init(&buf, data, sizeof(data));

	// Knightmare- removed this, put ESC-only substitute in keys.c
	/*if (cmd->buttons && cl.cinematictime > 0 && !cl.attractloop 
		&& cls.realtime - cl.cinematictime > 1000)
	{	// skip the rest of the cinematic
		SCR_FinishCinematic ();
	}*/

	// Begin a client move command
	MSG_WriteByte(&buf, clc_move);

	// Save the position for a checksum byte
	const int checksumIndex = buf.cursize;
	MSG_WriteByte(&buf, 0);

	// Let the server know what the last frame we got was, so the next message can be delta compressed
	if (cl_nodelta->value || !cl.frame.valid || cls.demowaiting)
		MSG_WriteLong(&buf, -1); // No compression
	else
		MSG_WriteLong(&buf, cl.frame.serverframe);

	// Send this and the previous cmds in the message, so if the last packet was dropped, it can be recovered
	cmdindex = (cls.netchan.outgoing_sequence - 2) & (CMD_BACKUP - 1);
	usercmd_t *cmd = &cl.cmds[cmdindex];

	usercmd_t nullcmd;
	memset(&nullcmd, 0, sizeof(nullcmd));
	MSG_WriteDeltaUsercmd(&buf, &nullcmd, cmd);
	usercmd_t *oldcmd = cmd;

	cmdindex = (cls.netchan.outgoing_sequence - 1) & (CMD_BACKUP - 1);
	cmd = &cl.cmds[cmdindex];
	MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd);
	oldcmd = cmd;

	cmdindex = (cls.netchan.outgoing_sequence) & (CMD_BACKUP - 1);
	cmd = &cl.cmds[cmdindex];
	MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd);

	// Calculate a checksum over the move commands
	buf.data[checksumIndex] = COM_BlockSequenceCRCByte(buf.data + checksumIndex + 1, buf.cursize - checksumIndex - 1, cls.netchan.outgoing_sequence);

	// Deliver the message
	Netchan_Transmit(&cls.netchan, buf.cursize, buf.data);

	if (async) //mxd
	{
		// Init the current cmd buffer and clear it
		cmd = &cl.cmds[cls.netchan.outgoing_sequence & (CMD_BACKUP - 1)];
		memset(cmd, 0, sizeof(*cmd));
	}
}