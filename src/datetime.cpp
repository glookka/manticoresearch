//
// Copyright (c) 2024, Manticore Software LTD (https://manticoresearch.com)
// All rights reserved
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//

#include "datetime.h"

#include "cctz/time_zone.h"
#include <stdlib.h>
#include <filesystem>

static bool g_bIntialized = false;
static bool g_bTimeZoneUTC = false;
static bool g_bTimeZoneSet = false;
static cctz::time_zone g_hTimeZone, g_hTimeZoneLocal, g_hTimeZoneUTC;
namespace filesystem = std::filesystem;


static void CheckForUTC()
{
	g_bTimeZoneUTC = !strcasecmp ( g_hTimeZone.name().c_str(), "UTC" );
}

#if !_WIN32
static bool DeterminePathToTimeZone ( filesystem::path & tPathToTimeZone, std::string & sTimeZoneName, CSphString & sWarning )
{
	const char * szTZ = getenv("TZ");
	if ( szTZ )
	{
		sWarning.SetSprintf ( "Unable to determine local time zone from TZ '%s'", szTZ );

		if ( *szTZ==':' )
			++szTZ;

		tPathToTimeZone = szTZ;
		sTimeZoneName = szTZ;

		return true;
	}

	tPathToTimeZone = "/etc/localtime";
	if ( !filesystem::exists(tPathToTimeZone) )
	{
		sWarning.SetSprintf ( "Unable to determine local time zone from file '%s'", tPathToTimeZone.string().c_str() );
		return false;
	}

	if ( std::filesystem::is_symlink(tPathToTimeZone) )
	{
		tPathToTimeZone = filesystem::read_symlink(tPathToTimeZone);
		if ( tPathToTimeZone.is_relative() )
			tPathToTimeZone = ( filesystem::path("/etc") / tPathToTimeZone ).lexically_normal();
	}

	return true;
}


static bool CheckRelativePath ( const filesystem::path & tPath, const std::string & sTimeZoneName, CSphString & sRes )
{
	if ( !tPath.empty() && *tPath.begin() != ".." && *tPath.begin() != "." )
	{
		sRes = sTimeZoneName.empty() ? tPath.string().c_str() : sTimeZoneName.c_str();
		return true;
	}

	return false;
}


static CSphString GetLocalTimeZoneName ( CSphString & sWarning )
{
	const char * szTZDIR = getenv("TZDIR");
	filesystem::path tTimeZoneDir = "/usr/share/zoneinfo/";
	if ( szTZDIR )
		tTimeZoneDir = szTZDIR;

	filesystem::path tPathToTimeZone;
	std::string sTimeZoneName;
	CSphString sWarningText;
	if ( !DeterminePathToTimeZone ( tPathToTimeZone, sTimeZoneName, sWarningText ) )
	{
		sWarning = sWarningText;
		return "UTC";
	}

	CSphString sRes;
	tTimeZoneDir = filesystem::weakly_canonical(tTimeZoneDir);
	if ( CheckRelativePath ( tPathToTimeZone.lexically_relative(tTimeZoneDir), sTimeZoneName, sRes ) )
		return sRes;

	if ( !tPathToTimeZone.is_absolute() )
		tPathToTimeZone = tTimeZoneDir / tPathToTimeZone;

	tPathToTimeZone = filesystem::weakly_canonical(tPathToTimeZone);
	if ( CheckRelativePath ( tPathToTimeZone.lexically_relative(tTimeZoneDir), sTimeZoneName, sRes ) )
		return sRes;

	sWarning = sWarningText;
	return "UTC";
}
#endif

static void SetTimeZoneLocal ( CSphString & sWarning )
{
	CSphString sDirName;
	sDirName.SetSprintf ( "%s/tzdata", GET_FULL_SHARE_DIR() );

#if _WIN32
	_putenv_s ( "TZDIR", sDirName.cstr() );

	// use cctz's internal local time zone code
	g_hTimeZoneLocal = cctz::local_time_zone();
#else
	CSphString sZone = GetLocalTimeZoneName(sWarning);

	setenv ( "TZDIR", sDirName.cstr(), 1 );

	if ( !cctz::load_time_zone ( sZone.cstr(), &g_hTimeZoneLocal ) )
	{
		sWarning.SetSprintf ( "Unable to set local time zone '%s' (tried TZ dir: '%s'), using UTC", sZone.cstr(), sDirName.cstr() );
		g_hTimeZoneLocal = g_hTimeZoneUTC;
	}
#endif

	g_hTimeZone = g_hTimeZoneLocal;
	CheckForUTC();
}


void InitTimeZones ( CSphString & sWarning )
{
	cctz::load_time_zone ( "UTC", &g_hTimeZoneUTC );
	SetTimeZoneLocal ( sWarning );
	g_bIntialized = true;
}


bool SetTimeZone ( const char * szTZ, CSphString & sError )
{
	if ( !cctz::load_time_zone ( szTZ, &g_hTimeZone ) )
	{
		sError.SetSprintf ( "Unable to set time zone '%s'", szTZ );
		return false;
	}

	g_bTimeZoneSet = !g_hTimeZone.name().empty() && strcasecmp ( g_hTimeZone.name().c_str(), "UTC" );

	CheckForUTC();
	return true;
}


bool IsTimeZoneSet()
{
	return g_bTimeZoneSet;
}


CSphString GetTimeZone()
{
	if ( !g_bIntialized )
		return "";

	return g_hTimeZone.name().c_str();
}


cctz::civil_second ConvertTime ( time_t tTime )
{
	return cctz::convert ( std::chrono::system_clock::from_time_t(tTime), g_hTimeZone );
}


cctz::civil_second ConvertTimeLocal ( time_t tTime )
{
	return cctz::convert ( std::chrono::system_clock::from_time_t(tTime), g_hTimeZoneLocal );
}


cctz::civil_second ConvertTimeUTC ( time_t tTime )
{
	return cctz::convert ( std::chrono::system_clock::from_time_t(tTime), g_hTimeZoneUTC );
}


time_t ConvertTime ( const cctz::civil_second & tCS )
{
	return std::chrono::system_clock::to_time_t ( cctz::convert ( tCS, g_hTimeZone ) );
}


time_t PackLocalTimeAsUTC ( time_t tTime )
{
	if ( g_bTimeZoneUTC )
		return tTime;

	// convert time to local timezone
	auto tSec = cctz::convert ( std::chrono::system_clock::from_time_t(tTime), g_hTimeZone );

	// interpret it as absolute UTC time
	return cctz::convert ( tSec, g_hTimeZoneUTC ).time_since_epoch().count();
}


int GetWeekDay ( const cctz::civil_second & tTime, bool bSundayFirst )
{
	if ( !bSundayFirst )
		return (int)cctz::get_weekday(tTime) + 1;

	// make sunday #1
	int iWeekDay = (int)cctz::get_weekday(tTime) + 2;
	if ( iWeekDay==8 )
		return 1;

	return iWeekDay;
}


int GetYearDay ( const cctz::civil_second & tTime )
{
	return (int)cctz::get_yearday(tTime);
}


int GetQuarter ( const cctz::civil_second & tTime )
{
	return ( ( tTime.month() - 1 ) / 3 ) + 1;
}


static FORCE_INLINE bool IsLeapYear ( int iYear )
{
	return ( iYear & 3 ) == 0 && ( iYear % 100 != 0 || iYear % 400 == 0 );
}


bool IsLeapYear ( const cctz::civil_second & tTime )
{
	return IsLeapYear ( tTime.year() );
}


int	CalcNumYearDays ( const cctz::civil_second & tTime )
{
	return IsLeapYear(tTime) ? 366 : 365;
}


CSphString FormatTime ( time_t tTime, const char * szFmt )
{
	std::string sRes = cctz::format ( szFmt, std::chrono::system_clock::from_time_t(tTime), g_hTimeZone );
	return sRes.c_str();
}


int CalcYearMonth ( const cctz::civil_second & tTime )
{
	return tTime.year()*100 + tTime.month();
}


int CalcYearMonthDay ( const cctz::civil_second & tTime )
{
	return tTime.year()*10000 + tTime.month()*100 + tTime.day();
}


int CalcYearWeek ( const cctz::civil_second & tTime )
{
	int iPrevSunday = GetYearDay(tTime) - GetWeekDay ( tTime, true ) + 1;
	int iYear = tTime.year();
	if ( iPrevSunday<=0 ) // check if we crossed year boundary
	{
		// adjust day and year
		iPrevSunday += 365;
		iYear--;

		if ( IsLeapYear(iYear) )
			iPrevSunday++;
	}
	return iYear*1000 + iPrevSunday;
}


static int CalcDayNumber ( const cctz::civil_day & tTime )
{
	cctz::civil_day tFirstDay;
	return tTime - tFirstDay;
}


enum WeekFlags_e : uint32_t
{
	WEEK_FLAG_MONDAY_FIRST = 1,
	WEEK_FLAG_YEAR = 2,
	WEEK_FLAG_FIRST_WEEKDAY = 4
};


static uint32_t FixupWeekFlags ( uint32_t uFlags )
{
	uFlags &= 7;
	if ( !( uFlags & WEEK_FLAG_MONDAY_FIRST ) )
		uFlags ^= WEEK_FLAG_FIRST_WEEKDAY;

	return uFlags;
}


int CalcWeekNumber ( const cctz::civil_second & tTime, uint32_t uFlags )
{
	uFlags = FixupWeekFlags(uFlags);
	bool bStartsWithMonday = uFlags & WEEK_FLAG_MONDAY_FIRST;
	bool bWeekOfYear = uFlags & WEEK_FLAG_YEAR;
	bool bFirstWeekDay = uFlags & WEEK_FLAG_FIRST_WEEKDAY;

	cctz::civil_day tFirstDayOfYear ( tTime.year(), 1, 1 );
	int iNumDays = CalcDayNumber( cctz::civil_day(tTime) );
	int iNumDaysFirst = CalcDayNumber(tFirstDayOfYear);

	int iWeekday = GetWeekDay ( tFirstDayOfYear, !bStartsWithMonday ) - 1;
	int iYear = tTime.year();

	if ( tTime.month()==1 && tTime.day()<=7-iWeekday )
	{
		if ( !bWeekOfYear && ( ( bFirstWeekDay && iWeekday!=0) || (!bFirstWeekDay && iWeekday >= 4) ) )
			return 0;

		bWeekOfYear = true;
		iYear--;
		int iDays = CalcNumYearDays ( cctz::civil_year(iYear) );
		iNumDaysFirst -= iDays;
		iWeekday = ( iWeekday + 53*7 - iDays ) % 7;
	}

	int iDays = 0;
	if ( (bFirstWeekDay && iWeekday != 0) || (!bFirstWeekDay && iWeekday >= 4) )
		iDays = iNumDays - ( iNumDaysFirst + ( 7 - iWeekday ) );
	else
		iDays = iNumDays - ( iNumDaysFirst - iWeekday );

	if ( bWeekOfYear && iDays>=52*7 )
	{
		iWeekday = ( iWeekday + CalcNumYearDays ( cctz::civil_year(iYear) ) ) % 7;
		if ( ( !bFirstWeekDay && iWeekday < 4 ) || ( bFirstWeekDay && !iWeekday ) )
			return 1;
	}

	return iDays/7 + 1;
}
