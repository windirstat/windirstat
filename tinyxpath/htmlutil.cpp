/*
www.sourceforge.net/projects/tinyxpath
Copyright (c) 2002-2004 Yves Berquin (yvesb@users.sourceforge.net)

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
   \file htmlutil.cpp
   \author Yves Berquin
   HTML utilities for TinyXPath project
*/

#include "tinyxml.h"
#include "htmlutil.h"

/// Generate some indentation on the HTML or file output
void v_levelize (int i_level, FILE * Fp_out, bool o_html)
{
	int i_loop;
	for (i_loop = 0; i_loop < i_level; i_loop++)
		if (o_html)
   		fprintf (Fp_out, "&nbsp;&nbsp;&nbsp;");
		else
	   	fprintf (Fp_out, "   ");
}

/// Dumps an XML tree to an HTML document
/// \n This is a recursive function, called again at each element in the tree
void v_out_html (
	FILE * Fp_out,                   ///< Output HTML file
	const TiXmlNode * XNp_source,    ///< Input XML tree
	unsigned u_level)                ///< Current level
{
	const TiXmlNode * XNp_child;
	const TiXmlAttribute * XAp_att;

	XNp_child = XNp_source -> FirstChild ();
	while (XNp_child)
	{
      switch (XNp_child -> Type ())
      {
         case TiXmlNode::DOCUMENT :
			   fprintf (Fp_out, "\nStart document\n");
            break;

		   case TiXmlNode::ELEMENT :
			   v_levelize ((int) u_level, Fp_out, true);
			   fprintf (Fp_out, "&lt;");   // '<'
			   fprintf (Fp_out, "%s", XNp_child -> ToElement () -> Value ());
            // fprintf (Fp_out, "<small>[%d]</small>", XNp_child -> GetUserValue ());
			   XAp_att = XNp_child -> ToElement () -> FirstAttribute ();
			   while (XAp_att)
			   {
				   fprintf (Fp_out, " %s='%s'", XAp_att -> Name (), XAp_att -> Value ());
				   XAp_att = XAp_att -> Next ();
			   }
			   if (XNp_child -> FirstChild ())
			      fprintf (Fp_out, "&gt;<br>\n");   // '>\n'
			   else
				   fprintf (Fp_out, " /&gt;<br>\n");
            break;

		   case TiXmlNode::COMMENT :
			   fprintf (Fp_out, "&lt;!-- %s --&gt;<br>\n", XNp_child -> ToComment () -> Value ());
            break;
		   case TiXmlNode::TEXT :
			   fprintf (Fp_out, "%s\n", XNp_child -> ToText () -> Value ());
            break;
		   case TiXmlNode::UNKNOWN :
		   case TiXmlNode::DECLARATION :
            break;
		   default :
			   assert (false);
      }

		v_out_html (Fp_out, XNp_child, u_level + 1);

      switch (XNp_child -> Type ())
      {
         case TiXmlNode::DOCUMENT :
   			fprintf (Fp_out, "\nEnd document\n");
            break;
		   case TiXmlNode::ELEMENT :
			   if (XNp_child -> FirstChild ())
			   {
				   v_levelize ((int) u_level, Fp_out, true);
				   fprintf (Fp_out, "&lt;");
				   fprintf (Fp_out, "/%s", XNp_child -> ToElement () -> Value ());
				   fprintf (Fp_out, "&gt;<br>\n");   // '>\n'
			   }
            break;
         case TiXmlNode::COMMENT :
         case TiXmlNode::TEXT :
		   case TiXmlNode::UNKNOWN :
		   case TiXmlNode::DECLARATION :
         default :
            break;
      }
		XNp_child = XNp_child -> NextSibling ();
	}
}
