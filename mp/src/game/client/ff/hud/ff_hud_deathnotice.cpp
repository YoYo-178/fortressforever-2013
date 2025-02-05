//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Draws CSPort's death notices
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "c_playerresource.h"
#include "clientmode_ff.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>
#include "c_ff_player.h"
#include "c_team.h"
#include "ff_gamerules.h"
#include "ff_shareddefs.h"
#include "ff_hud_chat.h" // custom team colors

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar hud_deathnotice_time( "hud_deathnotice_time", "6", FCVAR_ARCHIVE );
ConVar hud_deathnotice_selfonly( "hud_deathnotice_selfonly", "0", FCVAR_ARCHIVE );
ConVar hud_deathnotice_highlightself( "hud_deathnotice_highlightself", "1", FCVAR_ARCHIVE );
ConVar hud_deathnotice_assister_color_modifier( "hud_deathnotice_assister_color_modifier", "0.9", FCVAR_ARCHIVE, "Multiplier for the RGB and alpha values of the assister's name in deathnotice messages" );
ConVar cl_spec_killbeep( "cl_spec_killbeep", "1", FCVAR_ARCHIVE, "Determines whether or not the kill beep gets played while spectating someone in first-person mode" );

extern ConVar cl_killbeepwav;

#define MAX_OBJECTIVE_TEXT_LENGTH 48
#define DEATHNOTICE_ASSIST_SEPARATOR L" + " // this is a widechar string constant
#define DEATHNOTICE_COLOR_DEFAULT Color( 255, 80, 0, 255 )
#define DEATHNOTICE_COLOR_TEAMKILL Color(0, 185, 0, 250)

// Player entries in a death notice
struct DeathNoticePlayer
{
	char		szName[MAX_PLAYER_NAME_LENGTH];
	int			iEntIndex;
};

// Contents of each entry in our list of death notices
struct DeathNoticeItem 
{
	DeathNoticePlayer	Killer;
	DeathNoticePlayer   Victim;
	DeathNoticePlayer   Assister;
	CHudTexture *iconDeath; // draws before victims name
	CHudTexture *iconBuildable; // draws after victim name, if it exists
	CHudTexture *iconObjective; // draws before "victims" name
	CHudTexture *iconDeathModifier; // draws after the weapon icon and before the victims name
	char		objectiveText[MAX_OBJECTIVE_TEXT_LENGTH]; // "victim" has capped the flag
	int			iSuicide;
	float		flDisplayTime;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudDeathNotice : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudDeathNotice, vgui::Panel );
public:
	CHudDeathNotice( const char *pElementName );
	void Init( void );
	void VidInit( void );
	virtual bool ShouldDraw( void );
	virtual void Paint( void );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	void SetColorForNoticePlayer( int iTeamNumber );
	void RetireExpiredDeathNotices( void );
	void DrawObjectiveBackground( int xStart, int yStart, int xEnd, int yEnd );
	void DrawHighlightBackground( int xStart, int yStart, int xEnd, int yEnd );
	void DrawPlayerAndAssister( int &x, int &y, wchar_t* playerName, int iPlayerTeam, wchar_t* assisterName, int iAssisterTeam );
	
	virtual void FireGameEvent( IGameEvent * event );

private:

	CPanelAnimationVarAliasType( float, m_flLineHeight, "LineHeight", "15", "proportional_float" );

	CPanelAnimationVar( float, m_flMaxDeathNotices, "MaxDeathNotices", "4" );

	CPanelAnimationVar( bool, m_bRightJustify, "RightJustify", "1" );

	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HudNumbersTimer" );

	CPanelAnimationVar( Color, m_HighlightColor, "HighlightColor", "0 0 0 180" );
	CPanelAnimationVar( Color, m_HighlightBorderColor, "HighlightBorderColor", "HUD_Border_Default" );
	CPanelAnimationVar( Color, m_ObjectiveNoticeColor, "ObjectiveNoticeColor", "0 0 0 180" );

	// Texture for skull symbol
	CHudTexture		*m_iconD_skull;

	CUtlVector<DeathNoticeItem> m_DeathNotices;
};

using namespace vgui;

DECLARE_HUDELEMENT( CHudDeathNotice );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudDeathNotice::CHudDeathNotice( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "HudDeathNotice" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_iconD_skull = NULL;

	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::Init( void )
{
	gameeventmanager->AddListener(this, "player_death", false );
	gameeventmanager->AddListener( this, "dispenser_killed", false );
	gameeventmanager->AddListener( this, "sentrygun_killed", false );
	gameeventmanager->AddListener( this, "mancannon_killed", false );
	gameeventmanager->AddListener( this, "objective_event", false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::VidInit( void )
{
	m_iconD_skull = gHUD.GetIcon( "d_skull" );
	m_DeathNotices.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Draw if we've got at least one death notice in the queue
//-----------------------------------------------------------------------------
bool CHudDeathNotice::ShouldDraw( void )
{
	return ( CHudElement::ShouldDraw() && ( m_DeathNotices.Count() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::SetColorForNoticePlayer( int iTeamNumber )
{
	surface()->DrawSetTextColor( GetCustomClientColor( -1, iTeamNumber ) );
}

void CHudDeathNotice::DrawObjectiveBackground( int xStart, int yStart, int xEnd, int yEnd )
{
	surface()->DrawSetColor( m_ObjectiveNoticeColor );
	surface()->DrawFilledRect( xStart, yStart, xEnd, yEnd );
}

void CHudDeathNotice::DrawHighlightBackground( int xStart, int yStart, int xEnd, int yEnd )
{
	surface()->DrawSetColor( m_HighlightColor );
	surface()->DrawFilledRect( xStart, yStart, xEnd, yEnd );

	surface()->DrawSetColor( m_HighlightBorderColor );
	surface()->DrawOutlinedRect( xStart, yStart, xEnd, yEnd );
	surface()->DrawOutlinedRect( xStart - 1, yStart - 1, xEnd + 1, yEnd + 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDeathNotice::Paint()
{
	if ( !m_iconD_skull )
		return;

	int yStart = GetClientModeFFNormal()->GetDeathMessageStartHeight();

	surface()->DrawSetTextFont( m_hTextFont );
	surface()->DrawSetTextColor( GetCustomClientColor( -1, 0 ) );

	int iCount = m_DeathNotices.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		bool selfInvolved = m_DeathNotices[i].Victim.iEntIndex == GetLocalPlayerOrObserverTargetIndex() ||  m_DeathNotices[i].Killer.iEntIndex == GetLocalPlayerOrObserverTargetIndex() || m_DeathNotices[i].Assister.iEntIndex == GetLocalPlayerOrObserverTargetIndex();
		// if we should only draw notices that the local player is involved in and the local player isn't involved, then skip drawing this notice
		if (hud_deathnotice_selfonly.GetBool() && !selfInvolved)
			continue;

		CHudTexture *icon = m_DeathNotices[i].iconDeath;
		if ( !icon )
		{
			icon = m_DeathNotices[i].iconObjective;
			if (!icon)
				continue;

			wchar_t victim[ MAX_PLAYER_NAME_LENGTH ];
			wchar_t objectivetext[ 256 ];
			int iVictimTeam = 0;

			if( g_PR )
			{
				iVictimTeam = g_PR->GetTeam( m_DeathNotices[i].Victim.iEntIndex );
			}

			g_pVGuiLocalize->ConvertANSIToUnicode( m_DeathNotices[i].Victim.szName, victim, sizeof( victim ) );
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DeathNotices[i].objectiveText, objectivetext, sizeof( objectivetext ) );
			
			// Get the local position for this notice
			int victimStringWidth = UTIL_ComputeStringWidth( m_hTextFont, victim ) + UTIL_ComputeStringWidth( m_hTextFont, objectivetext ) + 5;
			int y = yStart + (m_flLineHeight * i);

			int iconWide;
			int iconTall;

			if( icon->bRenderUsingFont )
			{
				iconWide = surface()->GetCharacterWidth( icon->hFont, icon->cCharacterInFont );
				iconTall = surface()->GetFontTall( icon->hFont );
			}
			else
			{
				float scale = ( (float)ScreenHeight() / 480.0f );	//scale based on 640x480
				iconWide = (int)( scale * (float)icon->Width() );
				iconTall = (int)( scale * (float)icon->Height() );
			}
			
			int x;
			if ( m_bRightJustify )
			{
				x =	GetWide() - victimStringWidth - iconWide - 5;
			}
			else
			{
				x = 0;
			}
			
			// --> Mirv: Shove over a bit
			y += 16;
			x -= 28;
			// <--
			
#ifdef WIN32
			int x_start = x - 5;
			int y_start = y - (iconTall / 4) - 3;
			int y_end = y + iconTall/2 + 6;
#else
			int x_start = x - 14;
			int y_start = y - (iconTall / 4) - 6;
			int y_end = y + iconTall/2 + 12;
#endif

			int x_end = x + iconWide + 5 + victimStringWidth + 10;

			if (hud_deathnotice_highlightself.GetBool() && selfInvolved)
				DrawHighlightBackground(x_start, y_start, x_end, y_end);
			else
				DrawObjectiveBackground(x_start, y_start, x_end, y_end);
			
			// Draw death weapon
			//If we're using a font char, this will ignore iconTall and iconWide
			icon->DrawSelf( x, y - (iconTall / 4), iconWide, iconTall, DEATHNOTICE_COLOR_DEFAULT );
			x += iconWide + 5;		// |-- Mirv: 5px gap

			SetColorForNoticePlayer( iVictimTeam );

			// Draw victims name
			surface()->DrawSetTextPos( x, y );
			surface()->DrawSetTextFont( m_hTextFont );	//reset the font, draw icon can change it
			surface()->DrawUnicodeString( victim );
			surface()->DrawGetTextPos( x, y );

			x += 5;		// |-- Mirv: 5px gap

			// Draw objective text
			surface()->DrawSetTextColor( Color( 255, 80, 0, 255 ) );
			surface()->DrawSetTextPos( x, y );
			surface()->DrawSetTextFont( m_hTextFont );	//reset the font, draw icon can change it
			surface()->DrawUnicodeString( objectivetext );
			surface()->DrawGetTextPos( x, y );

		}
		else
		{
			bool hasAssister = m_DeathNotices[i].Assister.iEntIndex != -1;
			wchar_t victim[ MAX_PLAYER_NAME_LENGTH ];
			wchar_t killer[ MAX_PLAYER_NAME_LENGTH ];
			wchar_t assister[ MAX_PLAYER_NAME_LENGTH ];
		
			// Get the team numbers for the players involved
			int iKillerTeam = 0;
			int iVictimTeam = 0;
			int iAssisterTeam = 0;

			if( g_PR )
			{
				iKillerTeam = g_PR->GetTeam( m_DeathNotices[i].Killer.iEntIndex );
				iVictimTeam = g_PR->GetTeam( m_DeathNotices[i].Victim.iEntIndex );
				if (hasAssister)
				{
					iAssisterTeam = g_PR->GetTeam( m_DeathNotices[i].Assister.iEntIndex );
				}
			}

			g_pVGuiLocalize->ConvertANSIToUnicode( m_DeathNotices[i].Victim.szName, victim, sizeof( victim ) );
			g_pVGuiLocalize->ConvertANSIToUnicode( m_DeathNotices[i].Killer.szName, killer, sizeof( killer ) );
			
			if ( hasAssister )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( m_DeathNotices[i].Assister.szName, assister, sizeof( assister ) );
			}

			// Get the local position for this notice
			int victimStringWidth = UTIL_ComputeStringWidth( m_hTextFont, victim );
			int killerStringWidth = UTIL_ComputeStringWidth( m_hTextFont, killer );
			int assistSeparatorStringWidth = hasAssister ? UTIL_ComputeStringWidth( m_hTextFont, DEATHNOTICE_ASSIST_SEPARATOR ) : 0;
			int assisterStringWidth = hasAssister ? UTIL_ComputeStringWidth( m_hTextFont, assister ) : 0;
			int killerAndAssisterStringWidth = killerStringWidth + assistSeparatorStringWidth + assisterStringWidth;
			int victimAndAssisterStringWidth = victimStringWidth + (m_DeathNotices[i].iSuicide ? (assistSeparatorStringWidth + assisterStringWidth) : 0);
			int y = yStart + (m_flLineHeight * i);

			int iconWide;
			int iconTall;

			if( icon->bRenderUsingFont )
			{
				iconWide = surface()->GetCharacterWidth( icon->hFont, icon->cCharacterInFont );
				iconTall = surface()->GetFontTall( icon->hFont );
			}
			else
			{
				float scale = ( (float)ScreenHeight() / 480.0f );	//scale based on 640x480
				iconWide = (int)( scale * (float)icon->Width() );
				iconTall = (int)( scale * (float)icon->Height() );
			}
			
			CHudTexture *iconModifier = m_DeathNotices[i].iconDeathModifier;
			int iconModifierWide = 0;
			int iconModifierTall = 0;

			if (iconModifier)
			{
				if( iconModifier->bRenderUsingFont )
				{
					iconModifierWide = surface()->GetCharacterWidth( iconModifier->hFont, iconModifier->cCharacterInFont );
					iconModifierTall = surface()->GetFontTall( iconModifier->hFont );
				}
				else
				{
					float scale = ( (float)ScreenHeight() / 480.0f );	//scale based on 640x480
					iconModifierWide = (int)( scale * (float)iconModifier->Width() );
					iconModifierTall = (int)( scale * (float)iconModifier->Height() );
				}
			}

			int iconBuildableWide = 0;
			int iconBuildableTall = 0;

			// Add room for killed buildable icon
			CHudTexture *iconBuildable = m_DeathNotices[i].iconBuildable;
			if ( iconBuildable )
			{
				if( iconBuildable->bRenderUsingFont )
				{
					iconBuildableWide = surface()->GetCharacterWidth( iconBuildable->hFont, iconBuildable->cCharacterInFont );
					iconBuildableTall = surface()->GetFontTall( iconBuildable->hFont );
				}
				else
				{
					float scale = ( (float)ScreenHeight() / 480.0f );	//scale based on 640x480
					iconBuildableWide = (int)( scale * (float)iconBuildable->Width() );
					iconBuildableTall = (int)( scale * (float)iconBuildable->Height() );
				}
			}

			int x;
			if ( m_bRightJustify )
			{
				x =	GetWide() - victimAndAssisterStringWidth - iconWide - 5;

				// keep moving over for buildable icon
				x -= iconBuildableWide ? iconBuildableWide + 5 : 0;
				
				// keep moving over for modifier icon
				x -= iconModifierWide ? iconModifierWide + 5 : 0;
			}
			else
			{
				x = 0;
			}
			
			// --> Mirv: Shove over a bit
			y += 16;
			x -= 28;
			// <--
			
			if (hud_deathnotice_highlightself.GetBool() && selfInvolved)
			{
#ifdef WIN32
				int x_start = (m_DeathNotices[i].iSuicide) ? x - 5 : x - killerAndAssisterStringWidth - 5;
				
				int y_start = y - (iconTall / 4) - 3;
                                int y_end = y + iconTall/2 + 6;
#else
				int x_start = (m_DeathNotices[i].iSuicide) ? x - 14 : x - killerAndAssisterStringWidth - 14;
				
				int y_start = y - (iconTall / 4) - 6;
				int y_end = y + iconTall/2 + 12;
#endif

                                int x_end = x + 5 + iconWide + 5 + victimAndAssisterStringWidth + 5 + ((iconBuildableWide) ? iconBuildableWide + 5 : 0) + ((iconModifierWide) ? iconModifierWide + 5 : 0);
                                
				DrawHighlightBackground(x_start, y_start, x_end, y_end);
			}

			// Only draw killers name if it wasn't a suicide
			if ( !m_DeathNotices[i].iSuicide )
			{
				if ( m_bRightJustify )
				{
					x -= killerAndAssisterStringWidth;
				}

				// Draw killer's name
				DrawPlayerAndAssister( x, y, killer, iKillerTeam, hasAssister ? assister : NULL, iAssisterTeam );

				x += 5;	// |-- Mirv: 5px gap
			}
			
			// Don't include self kills when determining if teamkill
			//bool bTeamKill = (iKillerTeam == iVictimTeam && m_DeathNotices[i].Killer.iEntIndex != m_DeathNotices[i].Victim.iEntIndex);
			bool bTeamKill = ( ( FFGameRules()->IsTeam1AlliedToTeam2( iKillerTeam, iVictimTeam ) == GR_TEAMMATE ) && ( !m_DeathNotices[i].iSuicide ) );
			
			// Draw death weapon
			//If we're using a font char, this will ignore iconTall and iconWide
			icon->DrawSelf( x, y - (iconTall / 4), iconWide, iconTall, bTeamKill ? DEATHNOTICE_COLOR_TEAMKILL : DEATHNOTICE_COLOR_DEFAULT );
			x += iconWide + 5;		// |-- Mirv: 5px gap
			
			// draw the death modifier icon
			if (iconModifier)
			{
				iconModifier->DrawSelf( x, y - (iconModifierTall / 4), iconModifierWide, iconModifierTall, bTeamKill ? DEATHNOTICE_COLOR_TEAMKILL : DEATHNOTICE_COLOR_DEFAULT );
				x += iconModifierWide + 5;		// |-- Mirv: 5px gap
			}

			// Draw victims name
			DrawPlayerAndAssister( x, y, victim, iVictimTeam, (m_DeathNotices[i].iSuicide && hasAssister) ? assister : NULL, iAssisterTeam );

			// draw a team colored buildable icon
			if (iconBuildable)
			{
				x += 5;
				iconBuildable->DrawSelf( x, y - (iconBuildableTall / 4), iconBuildableWide, iconBuildableTall, GetCustomClientColor( -1, iVictimTeam ) );
			}
		}
	}

	// Now retire any death notices that have expired
	RetireExpiredDeathNotices();
}

void CHudDeathNotice::DrawPlayerAndAssister( int &x, int &y, wchar_t* playerName, int iPlayerTeam, wchar_t* assisterName, int iAssisterTeam )
{
	SetColorForNoticePlayer( iPlayerTeam );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawSetTextFont( m_hTextFont );
	surface()->DrawUnicodeString( playerName );

	if (assisterName != NULL)
	{
		surface()->DrawSetTextColor( DEATHNOTICE_COLOR_DEFAULT );
		surface()->DrawUnicodeString( DEATHNOTICE_ASSIST_SEPARATOR );
		
		float assisterColorModifier = hud_deathnotice_assister_color_modifier.GetFloat();
		Color assisterColor = GetCustomClientColor( -1, iAssisterTeam );
		assisterColor.SetColor(
			assisterColor.r() * assisterColorModifier, 
			assisterColor.g() * assisterColorModifier, 
			assisterColor.b() * assisterColorModifier, 
			assisterColor.a() * assisterColorModifier
		);
		surface()->DrawSetTextColor( assisterColor );
		surface()->DrawUnicodeString( assisterName );
	}

	surface()->DrawGetTextPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: This message handler may be better off elsewhere
//-----------------------------------------------------------------------------
void CHudDeathNotice::RetireExpiredDeathNotices( void )
{
	// Loop backwards because we might remove one
	int iSize = m_DeathNotices.Size();
	for ( int i = iSize-1; i >= 0; i-- )
	{
		if ( m_DeathNotices[i].flDisplayTime < gpGlobals->curtime )
		{
			m_DeathNotices.Remove(i);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Server's told us that someone's died
//-----------------------------------------------------------------------------
void CHudDeathNotice::FireGameEvent( IGameEvent * event )
{
	if (!g_PR)
		return;

	if ( hud_deathnotice_time.GetFloat() == 0 )
		return;

	// the event should be "player_death"
	int killer = engine->GetPlayerForUserID( event->GetInt("attacker") );
	int victim = engine->GetPlayerForUserID( event->GetInt("userid") );
	int sglevel_killed = event->GetInt( "killedsglevel" );
	int sglevel_killer = event->GetInt( "killersglevel" );
	const char *killedwith = event->GetString( "weapon" );
	int bitsDamageType = event->GetInt( "damagetype" );
	//const char *objectivename = event->GetString( "eventname" );
	const char *objectivetext = event->GetString( "eventtext" );
	
	// Do we have too many death messages in the queue?
	if ( m_DeathNotices.Count() > 0 &&
		m_DeathNotices.Count() >= (int)m_flMaxDeathNotices )
	{
		// Remove the oldest one in the queue, which will always be the first
		m_DeathNotices.Remove(0);
	}
	
	// printed to the console
	char sDeathMsg[512];

	// objective notice
	if ( objectivetext && *objectivetext /*&& objectivename && *objectivename*/ )
	{
		/*
		// commented because we aren't using multiple objective notice icons
		char fullobjectivename[128];
		Q_snprintf( fullobjectivename, sizeof(fullobjectivename), "death_objective_%s", objectivename );
		*/

		// Get the name of the player
		const char *victim_name = g_PR->GetPlayerName( victim );
		
		if ( !victim_name )
			victim_name = "";
		
		// Make a new death notice
		DeathNoticeItem deathMsg;
		deathMsg.Killer.iEntIndex = killer;
		deathMsg.Victim.iEntIndex = victim;
		Q_strncpy( deathMsg.Killer.szName, "", MAX_PLAYER_NAME_LENGTH );
		Q_strncpy( deathMsg.Victim.szName, victim_name, MAX_PLAYER_NAME_LENGTH );
		Q_strncpy( deathMsg.objectiveText, objectivetext, MAX_OBJECTIVE_TEXT_LENGTH );
		deathMsg.flDisplayTime = gpGlobals->curtime + hud_deathnotice_time.GetFloat();
		deathMsg.iconObjective = gHUD.GetIcon( "death_objective" );
		deathMsg.iconDeath = NULL;
		
		/*
		deathMsg.iconObjective = gHUD.GetIcon( fullobjectivename );
		if ( !deathMsg.iconObjective )
		{
			// Can't find it, so use the default objective icon
			DevMsg("Default objective notice icon!" );
			deathMsg.iconDeath = gHUD.GetIcon("death_objective");
		}*/

		// Add it to our list of death notices
		m_DeathNotices.AddToTail( deathMsg );

		Q_snprintf( sDeathMsg, sizeof( sDeathMsg ), "%s %s\n", deathMsg.Victim.szName, deathMsg.objectiveText );
	}
	// death notice
	else
	{

		// Stuffs for handling buildable deaths
		bool bBuildableKilled = false;

		// headshot deaths
		bool bHeadshot = false;
		// backstab deaths
		bool bBackstab = false;

		// trigger hurt death
		bool bTriggerHurt = false;
		char tempTriggerHurtString[128];
		char fullkilledwith[128];

		if ( killedwith && *killedwith )
		{
			// #0001568: Falling into pitt damage shows electrocution icon, instead of falling damage icon.  Until now there was no way
			// to identify different types of trigger_hurt deaths, so I added a "damagetype" field to the player_death event.
			// if a trigger_hurt is the killer, check the damage type and append it to the killer's name in a temp string (post-fix append)
			// e.g. if it's DMG_FALL, it's falling damage, so the complete string becomes death_trigger_hurt_fall.  
			// Match up the corresponding icons in scripts/ff_hud_textures.txt		-> Defrag

			if( Q_stricmp( "trigger_hurt", killedwith ) == 0 )
			{
				bTriggerHurt = true;

				switch( bitsDamageType )
				{
				case DMG_FALL:
					Q_snprintf( tempTriggerHurtString, sizeof(tempTriggerHurtString), "%s_fall", killedwith );
					break;

				case DMG_DROWN:
					Q_snprintf( tempTriggerHurtString, sizeof(tempTriggerHurtString), "%s_drown", killedwith );
					break;

				case DMG_SHOCK:
					Q_snprintf( tempTriggerHurtString, sizeof(tempTriggerHurtString), "%s_shock", killedwith );
					break;

				default:
					Q_strncpy( tempTriggerHurtString, killedwith, sizeof(tempTriggerHurtString));
					break;
				}
			}	

			if( bTriggerHurt )	// copy different string if it's a trigger_hurt death (see above)
			{
				Q_snprintf( fullkilledwith, sizeof(fullkilledwith), "death_%s", tempTriggerHurtString );
			}
			else
			{
				Q_snprintf( fullkilledwith, sizeof(fullkilledwith), "death_%s", killedwith );
			}		
		}
		else
		{
			fullkilledwith[0] = 0;
		}

		// Get the names of the players
		const char *killer_name = g_PR->GetPlayerName( killer );
		const char *victim_name = g_PR->GetPlayerName( victim );

		//bool bTeamKill = (g_PR->GetTeam(killer) == g_PR->GetTeam(victim));
		bool bTeamKill = ( ( FFGameRules()->IsTeam1AlliedToTeam2( g_PR->GetTeam( killer ), g_PR->GetTeam( victim ) ) == GR_TEAMMATE ) && ( killer != victim ) );

		if ( !killer_name )
			killer_name = "";
		if ( !victim_name )
			victim_name = "";

		// going to make these use icons instead of text
	/*
		// Buildable stuff
		char pszVictimMod[ MAX_PLAYER_NAME_LENGTH + 24 ];
		if( !Q_strcmp( event->GetName(), "dispenser_killed" ) ) 
		{
			bBuildableKilled = true;
			Q_snprintf( pszVictimMod, sizeof( pszVictimMod ), "%s's Dispenser", victim_name );
			victim_name = const_cast< char * >( pszVictimMod );
		}
		else if( !Q_strcmp( event->GetName(), "sentrygun_killed" ) )
		{
			bBuildableKilled = true;
			Q_snprintf( pszVictimMod, sizeof( pszVictimMod ), "%s's Sentrygun", victim_name );
			victim_name = const_cast< char * >( pszVictimMod );
		}
	*/

		// Make a new death notice
		DeathNoticeItem deathMsg;
		deathMsg.Killer.iEntIndex = killer;
		deathMsg.Victim.iEntIndex = victim;
		Q_strncpy( deathMsg.Killer.szName, killer_name, MAX_PLAYER_NAME_LENGTH );
		Q_strncpy( deathMsg.Victim.szName, victim_name, MAX_PLAYER_NAME_LENGTH );
		deathMsg.flDisplayTime = gpGlobals->curtime + hud_deathnotice_time.GetFloat();

		if (bitsDamageType & DMG_AIRSHOT)
			deathMsg.iconDeathModifier = gHUD.GetIcon( "death_airshot" );
		else
			deathMsg.iconDeathModifier = NULL;

		// buildable kills
		if( !Q_strcmp( event->GetName(), "sentrygun_killed" ) )
		{
			if (sglevel_killed == 1)
				deathMsg.iconBuildable = gHUD.GetIcon("death_sentrygun_level1");
			else if (sglevel_killed == 2)
				deathMsg.iconBuildable = gHUD.GetIcon("death_sentrygun_level2");
			else if (sglevel_killed == 3)
				deathMsg.iconBuildable = gHUD.GetIcon("death_sentrygun_level3");
			else
				deathMsg.iconBuildable = gHUD.GetIcon("death_weapon_deploysentrygun");
			bBuildableKilled = true;
		}
		else if( !Q_strcmp( event->GetName(), "dispenser_killed" ) ) 
		{
			deathMsg.iconBuildable = gHUD.GetIcon("death_weapon_deploydispenser");
			bBuildableKilled = true;
		}
		else if( !Q_strcmp( event->GetName(), "mancannon_killed" ) )
		{
			deathMsg.iconBuildable = gHUD.GetIcon( "death_weapon_deploymancannon");
			bBuildableKilled = true;
		}
		else
			deathMsg.iconBuildable = NULL;
		
		// old suicide logic:
		deathMsg.iSuicide = ( !killer || ( ( killer == victim ) && ( !bBuildableKilled ) ) );

		deathMsg.Assister.iEntIndex = -1;
		int killAssisterUserID = event->GetInt( "killassister", -1 );
		if (killAssisterUserID != -1)
		{
			deathMsg.Assister.iEntIndex = engine->GetPlayerForUserID(killAssisterUserID);
			Q_strncpy( deathMsg.Assister.szName, g_PR->GetPlayerName( deathMsg.Assister.iEntIndex ), MAX_PLAYER_NAME_LENGTH );
		}

		// only consider suicide if there was no kill assist
		//deathMsg.iSuicide = deathMsg.Assister.iEntIndex == -1 && ( !killer || ( ( killer == victim ) && ( !bBuildableKilled ) ) );

		// 0000336: If we have a Detpack...
		// NOTE: may need these changes for the SG and Dispenser in order for the death status icons to work right
		if (Q_stricmp(killedwith, "Detpack") == 0)
		{
			deathMsg.iconDeath = gHUD.GetIcon("death_weapon_deploydetpack");
		}
		// 0001292: If we have a Dispenser
		else if (Q_stricmp(killedwith, "Dispenser") == 0)
		{
			deathMsg.iconDeath = gHUD.GetIcon("death_weapon_deploydispenser");
		}
		// 0001292: If we have a Sentrygun
		else if (Q_stricmp(killedwith, "Sentrygun") == 0)
		{
			if (sglevel_killer == 1)
				deathMsg.iconDeath = gHUD.GetIcon("death_sentrygun_level1");
			else if (sglevel_killer == 2)
				deathMsg.iconDeath = gHUD.GetIcon("death_sentrygun_level2");
			else if (sglevel_killer == 3)
				deathMsg.iconDeath = gHUD.GetIcon("death_sentrygun_level3");
			else
				deathMsg.iconDeath = gHUD.GetIcon("death_weapon_deploysentrygun");
		}
		// only 1 weapon can kill with a headshot
		else if (Q_stricmp(killedwith, "BOOM_HEADSHOT") == 0)
		{
			// need the _weapon_sniperrifle in case people create "death_notice" entries in other weapon scripts...we just want the sniper rifle's
			deathMsg.iconDeath = gHUD.GetIcon("death_BOOM_HEADSHOT_weapon_sniperrifle");
			bHeadshot = true;
		}
		// only 1 weapon can kill with a backstab
		else if (Q_stricmp(killedwith, "backstab") == 0)
		{
			// need the _weapon_knife in case people create "death_notice" entries in other weapon scripts...we just want the knife's
			deathMsg.iconDeath = gHUD.GetIcon("death_backstab_weapon_knife");
			bBackstab = true;
		}
		// sg det
		else if (Q_stricmp(killedwith, "sg_det") == 0)
		{
			if (sglevel_killer == 1)
				deathMsg.iconDeath = gHUD.GetIcon("death_sentrygun_level1_det");
			else if (sglevel_killer == 2)
				deathMsg.iconDeath = gHUD.GetIcon("death_sentrygun_level2_det");
			else if (sglevel_killer == 3)
				deathMsg.iconDeath = gHUD.GetIcon("death_sentrygun_level3_det");
			else
				deathMsg.iconDeath = gHUD.GetIcon("death_weapon_deploysentrygun");
		}
		else if (Q_stricmp(killedwith, "grenade_napalmlet") == 0)
		{
			deathMsg.iconDeath = gHUD.GetIcon("death_grenade_napalm");
		}
		else if (Q_stricmp(killedwith, "headcrush") == 0)
		{
			deathMsg.iconDeath = gHUD.GetIcon("death_headcrush");
		}
		else if (Q_stricmp(killedwith, "weapon_railgun_bounce1") == 0)
		{
			deathMsg.iconDeath = gHUD.GetIcon("death_railgun_bounce1");
		}
		else if (Q_stricmp(killedwith, "weapon_railgun_bounce2") == 0)
		{
			deathMsg.iconDeath = gHUD.GetIcon("death_railgun_bounce2");
		}
		else
		{
			// Try and find the death identifier in the icon list
 			DevMsg("Death BY: %s.\n",fullkilledwith );
			deathMsg.iconDeath = gHUD.GetIcon( fullkilledwith );
		}

		// Show weapon if it was a suicide too
		if ( !deathMsg.iconDeath /*|| deathMsg.iSuicide*/ )
		{
			// Can't find it, so use the default skull & crossbones icon
			DevMsg("Default death icon!" );
			deathMsg.iconDeath = gHUD.GetIcon("death_world");
		}

		// Add it to our list of death notices
		m_DeathNotices.AddToTail( deathMsg );

		// Record the death notice in the console
		if ( deathMsg.iSuicide )
		{
			if ( !strcmp( fullkilledwith, "d_worldspawn" ) )
			{
				Q_snprintf( sDeathMsg, sizeof( sDeathMsg ), "%s died", deathMsg.Victim.szName );
			}
			else	//d_world
			{
				Q_snprintf( sDeathMsg, sizeof( sDeathMsg ), "%s suicided", deathMsg.Victim.szName );
			}

			if ( deathMsg.Assister.iEntIndex != -1 )
			{
				Q_strncat( sDeathMsg, VarArgs( ", assisted by %s.\n", deathMsg.Assister.szName ), sizeof ( sDeathMsg ), COPY_ALL_CHARACTERS );
			}
			else
			{
				// we still need to add new lien to msg
				Q_strncat( sDeathMsg, ".\n", sizeof ( sDeathMsg ), COPY_ALL_CHARACTERS );
			}
		}
		else
		{
			Q_snprintf( sDeathMsg, sizeof( sDeathMsg ), "%s %skilled %s", deathMsg.Killer.szName, bTeamKill ? "team" : "", deathMsg.Victim.szName );

			if ( fullkilledwith && *fullkilledwith && (*fullkilledwith > 13 ) )
			{
				Q_strncat( sDeathMsg, VarArgs( " with %s%s", fullkilledwith+6, (bitsDamageType & DMG_AIRSHOT ? " (airshot)" : "") ), sizeof( sDeathMsg ), COPY_ALL_CHARACTERS );
			}

			if ( deathMsg.Assister.iEntIndex != -1 )
			{
				Q_strncat( sDeathMsg, VarArgs( ", assisted by %s.\n", deathMsg.Assister.szName ), sizeof ( sDeathMsg ), COPY_ALL_CHARACTERS );
			}
			else
			{
				// we still need to add new lien to msg
				Q_strncat( sDeathMsg, ".\n", sizeof ( sDeathMsg ), COPY_ALL_CHARACTERS );
			}

		}

		// play the killbeep
		C_FFPlayer *pLocalPlayer = C_FFPlayer::GetLocalFFPlayerOrObserverTarget();
		if ( pLocalPlayer && killer == GetLocalPlayerOrObserverTargetIndex() && (pLocalPlayer->IsLocalPlayer() || cl_spec_killbeep.GetBool()) )
		{
			char buf[MAX_PATH];
			Q_snprintf( buf, MAX_PATH - 1, "player/deathbeep/%s.wav", cl_killbeepwav.GetString() );

			CPASAttenuationFilter filter(pLocalPlayer, buf);

			EmitSound_t params;
			params.m_pSoundName = buf;
			params.m_flSoundTime = 0.0f;
			params.m_pflSoundDuration = NULL;
			params.m_bWarnOnDirectWaveReference = false;
			params.m_nChannel = CHAN_STATIC;

			if ( bTeamKill )
			{
				params.m_nPitch = 88;
				params.m_flVolume = VOL_NORM;
			}
			else if ( deathMsg.iSuicide )
			{
				params.m_nPitch = 94;
				params.m_flVolume = 0.5f;
			}
			else if ( bBuildableKilled )
			{
				params.m_nPitch = 106;
				params.m_flVolume = VOL_NORM;
			}
			else if ( bHeadshot )
			{
				params.m_nPitch = 112;
				params.m_flVolume = 0.5f;
			}
			else if ( bBackstab )
			{
				params.m_nPitch = 112;
				params.m_flVolume = 0.5f;
			}
			else
			{
				params.m_nPitch = PITCH_NORM;
				params.m_flVolume = VOL_NORM;
			}

			pLocalPlayer->EmitSound(filter, pLocalPlayer->entindex(), params);
		}
	}

	Msg( "%s", sDeathMsg );
}



