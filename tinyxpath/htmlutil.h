/*
www.sourceforge.net/projects/tinyxpath
Copyright (c) 2002 Yves Berquin (yvesb@users.sourceforge.net)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

/**
   \file htmlutil.h
   \author Yves Berquin
   HTML utilities for TinyXPath project
*/
#ifndef __HTMLUTIL_H
#define __HTMLUTIL_H

#include "tinyxml.h"

extern void v_out_html (FILE * Fp_out, const TiXmlNode * XNp_source, unsigned u_level);
extern void v_levelize (int i_level, FILE * Fp_out = stdout, bool o_html = false);

#endif
