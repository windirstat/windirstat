// wds_constants.cpp
//
// WinDirStat - Directory Statistics
// Copyright (C) 2014 Oliver Schneider (assarbad.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author(s): - assarbad -> http://windirstat.info/contact/oliver/
//

#include "stdafx.h"
#include "wds_constants.h"

namespace wds
{
    demangle_t translator_demangle_email[] =
    {
        { L'/', L'-' },
        { L'*', L'.' },
        { L'#', L'\x0040' },
        { 0, 0 },
    };

    contact_t authors[] =
    {
        { L"Bernhard Seifert", 0, L"https://windirstat.info/contact/bernhard/", L"Creator of the project, original author" },
        { L"Oliver Schneider", 0, L"https://windirstat.info/contact/oliver/", L"Project maintainer" },
        { L"Chris Wimmer", 0, L"http://sourceforge.net/users/dezipaitor", L"Contributor" },
        { 0 },
    };

    translator_t translators[] =
    {
        {
            MAKELANGID(LANG_CZECH, SUBLANG_CZECH_CZECH_REPUBLIC),
            L"\x010Ce\x0161tina",
            L"Czech",
            L"cs",
            {
                { L"'TomR'", L"tomr#horses/online*cz", 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN),
            L"Deutsch",
            L"German",
            L"de-DE",
            {
                { L"WDS team", 0, 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            L"English",
            L"English",
            L"en-US",
            {
                { L"WDS team", 0, 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH),
            L"Espa\x00F1ol",
            L"Spanish",
            L"es",
            {
                { L"Sergio Omar Maurelli", L"sergiomaurelli#hotmail*com", 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_FINNISH, SUBLANG_FINNISH_FINLAND),
            L"Suomi",
            L"Finnish",
            L"fi",
            {
                { L"Markus Hietaranta", L"markus*hietaranta#gmail*com", 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH),
            L"Fran\x00E7ais",
            L"French",
            L"fr",
            {
                { L"Pascal Delrot", L"tigroo#users*sourceforge*net", 0, 0 },
                { L"'UltraSam'", L"sam*fd#wanadoo*fr", 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_HUNGARIAN, SUBLANG_HUNGARIAN_HUNGARY),
            L"Magyar",
            L"Hungarian",
            L"hu",
            {
                { L"'Leonard Nimoy'", L"sorge#freemail*hu", 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN),
            L"Italiano",
            L"Italian",
            L"it",
            {
                { L"Maria Antonietta Ricagno", L"ricagno#antotranslation*com", L"http://www.antotranslation.com", 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_DUTCH, SUBLANG_DUTCH),
            L"Nederlands",
            L"Dutch (Netherlands)",
            L"nl",
            {
                { L"Gerben Wieringa", L"gerbenwieringa#gmail*com", 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_POLISH, SUBLANG_POLISH_POLAND),
            L"Polski",
            L"Polish",
            L"pl",
            {
                { L"Dariusz Ma\x0142" L"achowski", L"d_malachowski#pf*pl", 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN),
            L"Portugu\x00EAs do Brasil",
            L"Portuguese (Brazil)",
            L"pt-BR",
            {
                { L"Eliezer Riani de Andrade", L"eliezer#rilaser*com*br", 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA),
            L"\x0420\x0443\x0441\x0441\x043A\x0438\x0439",
            L"Russian",
            L"ru",
            {
                { L"Sergiy '\x041F\x043E\x043B\x0451\x0442' Polyetayev", L"s_polyetayev#hotmail*com", 0, 0 },
                { 0 },
            },
        },
        {
            MAKELANGID(LANG_ESTONIAN, SUBLANG_ESTONIAN_ESTONIA),
            L"Eesti",
            L"Estonian",
            L"et",
            {
                { L"'Logard'", 0, 0 },
                { 0 },
            },
        },
        {
            0 /* end marker */
        },
    };
}
