// common_rsrcstr.h
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> bseifert@users.sourceforge.net, bseifert@daccord.net
//            - assarbad -> http://assarbad.net/en/contact
//
// $Header$

// This is a unicode encoded string with the names of the translators.
// Note that each language appears with its native name, followed by the
// 3-letter ISO code in brackets and the the translator(s).
// The languages are ordered by the Windows language identifier!
#define IDSS_TRANSLATORS \
		L"--- \x010Ce\x0161tina (CES/CZE) ---\n\n'TomR'\n(mailto:tomr#horses-online.cz)\n\n\n" \
		L"--- Deutsch (DEU/GER) ---\n\nWDS team\n\n\n" \
		L"--- English (ENG) ---\n\nWDS team\n\n\n" \
		L"--- Espa\x00F1ol (ESL/SPA) ---\n\nSergio Omar Maurelli\n(mailto:sergiomaurelli#hotmail.com)\n\n\n" \
		L"--- Fran\x00E7ais(FRA/FRE) ---\n\nPascal Delrot\n(mailto:tigroo#users.sourceforge.net)\n'UltraSam'\n(mailto:sam.fd#wanadoo.fr)\n\n\n" \
		L"--- Magyar (HUN) ---\n\n'Leonard Nimoy'\n(mailto:sorge#freemail.hu)\n\n\n" \
		L"--- Italiano (ITA) ---\n\nMaria Antonietta Ricagno\n(mailto:ricagno#antotranslation.com\nhttp://www.antotranslation.com)\n\n\n" \
		L"--- Polski (POL) ---\n\nDariusz Ma\x0142achowski\n(mailto:d_malachowski#pf.pl)\n\n\n" \
		L"--- \x0420\x0443\x0441\x0441\x043A\x0438\x0439/Russkij (RUS) ---\n\nSergiy '\x041F\x043E\x043B\x0451\x0442' Polyetayev\n(mailto:s_polyetayev#hotmail.com)\n\n\n"

// Version of resource DLL
#define IDSS_RESOURCEDLL		"Resource Version 4"
// Version information in feedback always appears in English
#define IDSS_FROMsPLATFORMs		"From: %1!s!. Platform: %2!s!.\r\n\r\n"
#define IDSS_SEV_CRITICAL		"Critical Bug"
#define IDSS_SEV_GRAVE			"Serious Bug"
#define IDSS_SEV_NORMAL			"Bug"
#define IDSS_SEV_WISH			"Wish"
#define IDSS_SEV_FEEDBACK		"Feedback"

// $Log$
// Revision 1.6  2006/07/04 23:37:39  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
