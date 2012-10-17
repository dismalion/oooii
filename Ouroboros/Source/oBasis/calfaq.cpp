// $(noheader)
/*
 * CALFAQ version 1.1, 4 April 2008
 *
 * COPYRIGHT:
 *   These functions are Copyright (c) 2008 by Claus Tondering
 *   (claus@tondering.dk).
 *  
 * LICENSE:
 *   The code is distributed under the Boost Software License, which
 *   says:
 *  
 *     Boost Software License - Version 1.0 - August 17th, 2003
 *  
 *     Permission is hereby granted, free of charge, to any person or
 *     organization obtaining a copy of the software and accompanying
 *     documentation covered by this license (the "Software") to use,
 *     reproduce, display, distribute, execute, and transmit the
 *     Software, and to prepare derivative works of the Software, and
 *     to permit third-parties to whom the Software is furnished to do
 *     so, all subject to the following:
 *  
 *     The copyright notices in the Software and this entire
 *     statement, including the above license grant, this restriction
 *     and the following disclaimer, must be included in all copies of
 *     the Software, in whole or in part, and all derivative works of
 *     the Software, unless such copies or derivative works are solely
 *     in the form of machine-executable object code generated by a
 *     source language processor.
 *  
 *     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *     EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *     OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 *     NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 *     ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR
 *     OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 *     ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 *     USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * DESCRIPTION:
 *   These functions are an implementation in the C language of the
 *   formulas presented in the Calendar FAQ at
 *   http://www.tondering.dk/claus/calendar.html.
 *
 *   The implementation follows the formulas mentioned in version 2.9
 *   of the FAQ quite closely. The focus of the implementation is on
 *   simplicity and clarity. For this reason, no complex data
 *   structures or classes are used, nor has any attempt been made to
 *   optimize the code. Also, no verification of the input parameters
 *   is performed (except in the function simple_gregorian_easter).
 *
 *   All numbers (including Julian Day Numbers which current have
 *   values of almost 2,500,000) are assumed to be representable as
 *   variables of type 'int'.
 */

#include "calfaq.h"

/*
 * is_leap:
 * Determines if a year is a leap year.
 * Input parameters:
 *     Calendar style (JULIAN or GREGORIAN)
 *     Year (must be >0)
 * Returns:
 *     1 if the year is a leap year, 0 otherwise.
 *
 * Note: The algorithm assumes that AD 4 is a leap year. This may be
 * historically inaccurate. See the FAQ.
 *
 * Reference: Sections 2.1.1 and 2.2.1 of version 2.9 of the FAQ.
 */
int is_leap(int style, int year)
{
    return style==JULIAN
             ? year%4==0
             : (year%4==0 && year%100!=0) || year%400==0;
}


/*
 * days_in_month:
 * Calculates the number of days in a month.
 * Input parameters:
 *     Calendar style (JULIAN or GREGORIAN)
 *     Year (must be >0)
 *     Month (1..12)
 * Returns:
 *     The number of days in the month (28..31)
 */
int days_in_month(int style, int year, int month)
{
    static int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    return month==2 && is_leap(style,year)
             ? 29
             : days[month-1];
}


/*
 * solar_number:
 * Calculates the Solar Number of a given year.
 * Input parameter:
 *     Year (must be >0)
 * Returns:
 *     Solar Number (1..28)
 *
 * Reference: Section 2.4 of version 2.9 of the FAQ.
 */
int solar_number(int year)
{
    return (year + 8) % 28 + 1;
}


/*
 * day_of_week:
 * Calculates the weekday for a given date.
 * Input parameters:
 *     Calendar style (JULIAN or GREGORIAN)
 *     Year (must be >0)
 *     Month (1..12)
 *     Day (1..31)
 * Returns:
 *     0 for Sunday, 1 for Monday, 2 for Tuesday, etc.
 *
 * Reference: Section 2.6 of version 2.9 of the FAQ.
 */
int day_of_week(int style, int year, int month, int day)
{
    int a = (14 - month) / 12;
    int y = year - a;
    int m = month + 12*a - 2;
    return style== JULIAN
             ? (5 + day + y + y/4 + (31*m)/12) % 7
             : (day + y + y/4 - y/100 + y/400 + (31*m)/12) % 7;
}


/*
 * golden_number:
 * Calculates the Golden Number of a given year.
 * Input parameter:
 *     Year (must be >0)
 * Returns:
 *     Golden Number (1..19)
 *
 * Reference: Section 2.13.3 of version 2.9 of the FAQ.
 */
int golden_number(int year)
{
    return year%19 + 1;
}


/*
 * epact:
 * Calculates the Epact of a given year.
 * Input parameters:
 *     Calendar style (JULIAN or GREGORIAN)
 *     Year (must be >0)
 * Returns:
 *     Epact (1..30)
 *
 * Reference: Section 2.13.5 of version 2.9 of the FAQ.
 */
int epact(int style, int year)
{
    if (style==JULIAN) {
        int je = (11 * (golden_number(year)-1)) % 30;
        return je==0 ? 30 : je;
    }
    else {
        int century = year/100 + 1;
        int solar_eq = (3*century)/4;
        int lunar_eq = (8*century + 5)/25;
        int greg_epact = epact(JULIAN,year) - solar_eq + lunar_eq + 8;

        while (greg_epact>30)
            greg_epact -= 30;
        while (greg_epact<1)
            greg_epact += 30;

        return greg_epact;
    }
}


/*
 * paschal_full_moon:
 * Calculates the date of the Paschal full moon.
 * Input parameters:
 *     Calendar style (JULIAN or GREGORIAN)
 *     Year (must be >0)
 * Output parameters:
 *     Address of month of Paschal full moon (3..4)
 *     Address of day of Pascal full moon (1..31)
 *
 * Reference: Section 2.13.4 and 2.13.6 of version 2.9 of the FAQ.
 */
void paschal_full_moon(int style, int year, int *month, int *day)
{
    if (style==JULIAN) {
        static struct {
            int month;
            int day;
        } jul_pfm[] = {
            { 4,  5 }, { 3, 25 }, { 4, 13 }, { 4,  2 }, { 3, 22 },
            { 4, 10 }, { 3, 30 }, { 4, 18 }, { 4,  7 }, { 3, 27 },
            { 4, 15 }, { 4,  4 }, { 3, 24 }, { 4, 12 }, { 4,  1 },
            { 3, 21 }, { 4,  9 }, { 3, 29 }, { 4, 17 }
        };

        int gn = golden_number(year);
        *month = jul_pfm[gn-1].month;
        *day = jul_pfm[gn-1].day;
    }
    else {
        int gepact = epact(GREGORIAN,year);

        if (gepact<=12) {
            *month = 4;
            *day = 13-gepact;
        }
        else if (gepact<=23) {
            *month = 3;
            *day = 44-gepact;
        }
        else if (gepact==24) {
            *month = 4;
            *day = 18;
        }
        else if (gepact==25) {
            *month = 4;
            *day = golden_number(year)>11 ? 17 : 18;
        }
        else {
            *month = 4;
            *day = 43-gepact;
        }
    }
}


/*
 * easter:
 * Calculates the date of Easter Sunday.
 * Input parameters:
 *     Calendar style (JULIAN or GREGORIAN)
 *     Year (must be >0)
 * Output parameters:
 *     Address of month of Easter Sunday (3..4)
 *     Address of day of Easter Sunday (1..31)
 *
 * Reference: Section 2.13.7 of version 2.9 of the FAQ.
 */
void easter(int style, int year, int *month, int *day)
{
    int G, I, J, C, H, L;

    G = year % 19;

    if (style==JULIAN) {
        I = (19*G + 15) % 30;
        J = (year + year/4 + I) % 7;
    }
    else {
        C = year/100;
        H = (C - C/4 - (8*C+13)/25 + 19*G + 15) % 30;
        I = H - (H/28)*(1 - (29/(H + 1))*((21 - G)/11));
        J = (year + year/4 + I + 2 - C + C/4) % 7;
    }

    L = I - J;
    *month = 3 + (L + 40)/44;
    *day = L + 28 - 31*(*month/4);
}


/*
 * simple_gregorian_easter:
 * Calculates the date of Easter Sunday in the Gregorian calendar.
 * Input parameter:
 *     Year (must be in the range 1900..2099)
 * Output parameters:
 *     Address of month of Easter Sunday (3..4)
 *     Address of day of Easter Sunday (1..31)
 *
 * If the year is outside the legal range, *month is set to zero.
 *
 * Reference: Section 2.13.8 of version 2.9 of the FAQ.
 */
void simple_gregorian_easter(int year, int *month, int *day)
{
    int H, I, J, L;

    if (year<1900 || year>2099) {
        *month = 0;
        return;
    }

    H = (24 + 19*(year % 19)) % 30;
    I = H - H/28;
    J = (year + year/4 + I - 13) % 7;
    L = I - J;
    *month = 3 + (L + 40)/44;
    *day = L + 28 - 31*(*month/4);
}


/*
 * indiction:
 * Calculates the Indiction of a given year.
 * Input parameter:
 *     Year (must be >0)
 * Returns:
 *     Indiction (1..15)
 *
 * Reference: Section 2.15 of version 2.9 of the FAQ.
 */
int indiction(int year)
{
    return (year + 2) % 15 + 1;
}


/*
 * julian_period:
 * Calculates the year in the Julian Period corresponding to a given
 * year.
 * Input parameter:
 *    Year (must be in the range -4712..3267). The year 1 BC must be
 *        given as 0, the year 2 BC must be given as -1, etc.
 * Returns:
 *    The corresponding year in the Julian period
 *
 * Reference: Section 2.16 of version 2.9 of the FAQ.
 */
int julian_period(int year)
{
    return year+4713;
}


/*
 * date_to_jdn:
 * Calculates the Julian Day Number for a given date.
 * Input parameters:
 *     Calendar style (JULIAN or GREGORIAN)
 *     Year (must be > -4800). The year 1 BC must be given as 0, the
 *         year 2 BC must be given as -1, etc.
 *     Month (1..12)
 *     Day (1..31)
 * Returns:
 *     Julian Day Number
 *
 * Reference: Section 2.16.1 of version 2.9 of the FAQ.
 */
int date_to_jdn(int style, int year, int month, int day)
{
    int a = (14-month)/12;
    int y = year+4800-a;
    int m = month + 12*a - 3;
    return style==JULIAN
             ? day + (153*m+2)/5 + y*365 + y/4 - 32083
             : day + (153*m+2)/5 + y*365 + y/4 - y/100 + y/400 - 32045;
}
    

/*
 * jdn_to_date:
 * Calculates the date for a given Julian Day Number.
 * Input parameter:
 *     Calendar style (JULIAN or GREGORIAN)
 *     Julian Day Number
 * Output parameters:
 *     Address of year. The year 1 BC will be stored as 0, the year
 *         2 BC will be stored as -1, etc.
 *     Address of month (1..12)
 *     Address of day (1..31)
 *
 * Reference: Section 2.16.1 of version 2.9 of the FAQ.
 */
void jdn_to_date(int style, int JD, int *year, int *month, int *day)
{
    int b, c, d, e, m;

    if (style==JULIAN) {
        b = 0;
        c = JD + 32082;
    }
    else {
        int a = JD + 32044;
        b = (4*a+3)/146097;
        c = a - (b*146097)/4;
    }

    d = (4*c+3)/1461;
    e = c - (1461*d)/4;
    m = (5*e+2)/153;

    *day   = e - (153*m+2)/5 + 1;
    *month = m + 3 - 12*(m/10);
    *year  = b*100 + d - 4800 + m/10;
}


/*
 * week_number:
 * Calculates the ISO 8601 week number (and corresponding year) for a given
 * Gregorian date.
 * Input parameters:
 *     Year (must be >0)
 *     Month (1..12)
 *     Day
 * Output parameters:
 *     Address of week number (1..53)
 *     Address of corresponding year
 *
 * Reference: Section 7.8 of version 2.9 of the FAQ.
 */
void week_number(int year, int month, int day, int *week_number, int *week_year)
{
    int a, b, c, s, e, f, g, d, n;

    if (month<=2) {
        a = year-1;
        b = a/4 - a/100 + a/400;
        c = (a-1)/4 - (a-1)/100 + (a-1)/400;
        s = b-c;
        e = 0;
        f = day - 1 + 31*(month-1);
    }
    else {
        a = year;
        b = a/4 - a/100 + a/400;
        c = (a-1)/4 - (a-1)/100 + (a-1)/400;
        s = b-c;
        e = s+1;
        f = day + (153*(month-3)+2)/5 + 58 + s;
    }

    g = (a + b) % 7;
    d = (f + g - e) % 7;
    n = f + 3 - d;

    if (n<0) {
        *week_number = 53-(g-s)/5;
        *week_year = year-1;
    }
    else if (n>364+s) {
        *week_number = 1;
        *week_year = year+1;
    }
    else {
        *week_number = n/7 + 1;
        *week_year = year;
    }
}
