#ifndef FF_HUD_GRENADE1TIMER_H
#define FF_HUD_GRENADE1TIMER_H

#include "cbase.h"
#include <vgui_controls/Panel.h>

using namespace vgui;

#define GREN1_TIMER_BACKGROUND_TEXTURE "hud/Gren1TimerBG"
#define GREN1_TIMER_FOREGROUND_TEXTURE "hud/Gren1TimerFG"

extern Color GetCustomClientColor(int iPlayerIndex, int iTeamIndex/* = -1*/);

class CHudGrenade1Timer : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE(CHudGrenade1Timer, Panel);

public:
	CHudGrenade1Timer(const char *pElementName);
	~CHudGrenade1Timer( void );

	virtual void	Init( void );
	virtual void	VidInit( void );
	virtual void	Paint( void );
	virtual bool	ShouldDraw( void );

	void CacheTextures( void );

	void	SetTimer(float duration);
	bool	ActiveTimer( void ) const;
	void	ResetTimer( void );

	// Callback functions for setting
	void	MsgFunc_FF_Grenade1Timer( bf_read &msg );
	
	int ActiveTimerCount( void ) const;

private:
	typedef struct timer_s
	{
		float m_flStartTime;
		float m_flDuration;

		timer_s(float s, float d) 
		{
			m_flStartTime = s;
			m_flDuration = d;
		}

	} timer_t;

	CUtlLinkedList<timer_t> m_Timers;
	int m_iClass;
	int m_iPlayerTeam;
	bool m_fVisible;
	float m_flLastTime;

	CHudTexture *m_pIconTexture;

	CPanelAnimationVarAliasType(float, bar_xpos, "bar_xpos", "12", "proportional_float");
	CPanelAnimationVarAliasType(float, bar_ypos, "bar_ypos", "0", "proportional_float");

	CPanelAnimationVarAliasType(float, icon_xpos, "icon_xpos", "0", "proportional_float");
	CPanelAnimationVarAliasType(float, icon_ypos, "icon_ypos", "0", "proportional_float");

	CPanelAnimationVar(Color, icon_color, "icon_color", "HUD_Tone_Default");

	CHudTexture* m_pBGTexture;
	CHudTexture* m_pFGTexture;
};

#endif