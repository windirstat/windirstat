/*
   This is an example source code and a regression test program for the TinyXPath project
   We load a very small test.xml file and test the return value of the string variant of 
   TinyXPath against a known output.
   The LIBXML_CHECK define may be turned ON if we need to verify the output against libxml 
   (from the Gnome project).
*/
#include "xpath_static.h"
#include "htmlutil.h"
/*
 @history:
 
 Modified on 18 December 2006 by Aman Aggarwal
 ::Added test cases for translate()  
*/
static FILE * Fp_out_html;

// #define LIBXML_CHECK

static void v_out_one_line (const char * cp_expr, const char * cp_res, const char * cp_expected, bool o_ok)
{  
   fprintf (Fp_out_html, "<tr><td>%s</td>", cp_expr);
   if (o_ok)
      fprintf (Fp_out_html, "<td>%s</td><td>%s</td></tr>\n", cp_res, cp_expected);
   else
      fprintf (Fp_out_html, "<td><em>%s</em></td><td><em>%s</em></td></tr>\n", cp_res, cp_expected);
}

#ifdef LIBXML_CHECK
   #include "libxml/tree.h"
   #include "libxml/xpath.h"

   /// Return the text result (if any) of the xpath expression
   TIXML_STRING S_xpath_expr (const xmlDoc * Dp_ptr, const char * cp_xpath_expr)
   {
      xmlXPathObjectPtr XPOp_ptr;
      xmlXPathContextPtr XPCp_ptr;
      const xmlChar * Cp_ptr;
      TIXML_STRING S_out;
   
      S_out = "";
      if (Dp_ptr)
      {
         XPCp_ptr = xmlXPathNewContext ((xmlDoc *) Dp_ptr);
         if (XPCp_ptr)
         {
            // Evaluate 
            XPOp_ptr = xmlXPathEval ((const xmlChar *) cp_xpath_expr, XPCp_ptr);
            if (XPOp_ptr)
            {
               Cp_ptr = xmlXPathCastToString (XPOp_ptr);
               if (Cp_ptr)
                  S_out = (const char *) Cp_ptr;
               xmlXPathFreeObject (XPOp_ptr);
            }
         }
         if (XPCp_ptr)
            xmlXPathFreeContext (XPCp_ptr);
      }

      return S_out;
   }

   static void v_test_one_string_libxml (const TiXmlNode * XNp_root, const char * cp_expr, const xmlDoc * Dp_ptr)
   {
      TIXML_STRING S_expected, S_res;
      bool o_ok;

      S_expected = S_xpath_expr (Dp_ptr, cp_expr);
      printf ("-- expr : [%s] --\n", cp_expr);
      TinyXPath::xpath_processor xp_proc (XNp_root, cp_expr);
      S_res = xp_proc . S_compute_xpath ();
      o_ok = strcmp (S_res . c_str (), S_expected . c_str ()) == 0;
      v_out_one_line (cp_expr, S_res . c_str (), S_expected . c_str (), o_ok);
   }

#endif

static void v_test_one_string_tiny (const TiXmlNode * XNp_root, const char * cp_expr, const char * cp_expected)
{
   TIXML_STRING S_res;
   bool o_ok;

   printf ("-- expr : [%s] --\n", cp_expr);
   S_res = TinyXPath::S_xpath_string (XNp_root, cp_expr);
   o_ok = strcmp (S_res . c_str (), cp_expected) == 0;
   v_out_one_line (cp_expr, S_res . c_str (), cp_expected, o_ok);
}

#ifdef LIBXML_CHECK 
   #define v_test_one_string(x,y,z) v_test_one_string_libxml(x,y,Dp_doc)
#else
   #define v_test_one_string v_test_one_string_tiny
#endif


int main ()
{
   TiXmlDocument * XDp_doc;
   TiXmlElement * XEp_main;
   int i_res;
   char ca_res [80];
   bool o_ok, o_res;
   double d_out;

   TIXML_STRING S_res;

   
   XDp_doc = new TiXmlDocument;
   if (XDp_doc -> LoadFile ("openoff.xml"))
   {
      S_res = TinyXPath::S_xpath_string (XDp_doc -> RootElement (), "/office:document-content/office:body/office:text/text:p/text()");
      S_res = TinyXPath::S_xpath_string (XDp_doc -> RootElement (), "//text:sequence-decls/*[1]/@text:name");
   }   
   delete XDp_doc;

   XDp_doc = new TiXmlDocument;
   if (! XDp_doc -> LoadFile ("test.xml"))
   {
      printf ("Can't find test.xml !\n");
      return 99;
   }

   #ifdef LIBXML_CHECK
      xmlDoc * Dp_doc;
      Dp_doc = xmlParseFile ("test.xml");
   #endif

   Fp_out_html = fopen ("out.htm", "wt");
   if (! Fp_out_html)
      return 1;
   fprintf (Fp_out_html, "<html><head><title>Result</title>\n<style>");
   fprintf (Fp_out_html, "em{color: red;}</style>\n");
   fprintf (Fp_out_html, "</head><body>\n");

   fprintf (Fp_out_html, "<h1>TinyXPath examples / regression tests</h1>\n");
   fprintf (Fp_out_html, "<h2>Input XML tree</h2>\n");
   v_out_html (Fp_out_html, XDp_doc, 0);
   fprintf (Fp_out_html, "<br /><br />\n");
   fprintf (Fp_out_html, "<table border='1'><tr><th>Expression</th><th>Result</th><th>Expected (%s)</th></tr>\n",
      #ifdef LIBXML_CHECK
         "libXML"
      #else
         "compiled"
      #endif
         );

   XEp_main = XDp_doc -> RootElement ();

   fprintf (Fp_out_html, "<h2>Results</h2>\n");

   TiXmlElement * XEp_sub = XEp_main -> FirstChildElement ("b");

   v_test_one_string (XEp_main, "/a/*[name()!='b']", "x");
   v_test_one_string (XEp_main, "//b/@val", "123" ); 

   v_test_one_string (XEp_main, "//x/text()", "sub text");
   v_test_one_string (XEp_main, "//*/comment()", " -122.0 ");

   v_test_one_string (XEp_main, "( //dummy1 or  //dummy2  or /dummy/dummy2 or /a/b )", "true");
   v_test_one_string (XEp_main, "( //dummy1 or  //dummy2  or /dummy/dummy2  )", "false");
   
   v_test_one_string (XEp_main, "translate('aabbccdd','aaabc','AXYB')" , "AABBdd");
   v_test_one_string (XEp_main, "translate('aabbccdde','abcd','')" , "e");
	
   v_test_one_string (XEp_main, "translate('aabbccdd','','ASDF')" , "aabbccdd");
   v_test_one_string (XEp_main, "translate('aabbccdd','','')" , "aabbccdd");
	
   v_test_one_string (XEp_main, "translate('The quick brown fox jumps over the lazy dog','abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')" , "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG");

   v_test_one_string (XEp_main, "count(//*/comment())", "2");
   v_test_one_string (XEp_main, "sum(//@*)", "123");
   v_test_one_string (XEp_main, "sum(//*/comment())", "378");
   v_test_one_string (XEp_main, "true()", "true");
   v_test_one_string (XEp_main, "not(false())", "true");

   v_test_one_string (XEp_main, "count(//*[position()=2])", "2");
   v_test_one_string (XEp_main, "name(/*/*/*[position()=2])", "c");
   v_test_one_string (XEp_main, "name(/*/*/*[last()])", "d");

   v_test_one_string (XEp_main, "count(//c/following::*)", "2");
   v_test_one_string (XEp_main, "count(/a/b/b/following::*)", "3");
   v_test_one_string (XEp_main, "count(//d/preceding::*)", "2");
   v_test_one_string (XEp_main, "name(//attribute::*)", "val");
   v_test_one_string (XEp_main, "count(//b/child::*)", "3");
   v_test_one_string (XEp_main, "count(//x/ancestor-or-self::*)", "2");
   v_test_one_string (XEp_main, "count(//b/descendant-or-self::*)", "4");

   v_test_one_string (XEp_main, "count(//self::*)", "6");

   v_test_one_string (XEp_main, "count(/a/descendant::*)", "5");
   v_test_one_string (XEp_main, "count(/a/descendant::x)", "1");
   v_test_one_string (XEp_main, "count(/a/descendant::b)", "2");
   v_test_one_string (XEp_main, "count(/a/descendant::b[@val=123])", "1");
   v_test_one_string (XEp_main, "count(//c/ancestor::a)", "1");
   v_test_one_string (XEp_main, "name(//d/parent::*)", "b");
   v_test_one_string (XEp_main, "count(//c/ancestor::*)", "2");
   v_test_one_string (XEp_main, "name(/a/b/ancestor::*)", "a");
   v_test_one_string (XEp_main, "name(/a/b/c/following-sibling::*)", "d");
   v_test_one_string (XEp_main, "count(//b/following-sibling::*)", "3");
   v_test_one_string (XEp_main, "count(//b|//a)", "3");
   v_test_one_string (XEp_main, "count(//d/preceding-sibling::*)", "2");

   v_test_one_string (XEp_main, "-3 * 4", "-12");
   v_test_one_string (XEp_main, "-3.1 * 4", "-12.4");
   v_test_one_string (XEp_main, "12 div 5", "2.4");
   v_test_one_string (XEp_main, "3 * 7", "21");

   v_test_one_string (XEp_main, "-5.5 >= -5.5", "true");
   v_test_one_string (XEp_main, "-5.5 < 3", "true");
   v_test_one_string (XEp_main, "-6.0 < -7", "false");
   v_test_one_string (XEp_main, "12 < 14", "true");
   v_test_one_string (XEp_main, "12 > 14", "false");
   v_test_one_string (XEp_main, "14 <= 14", "true");

   v_test_one_string (XEp_main, "/a or /b", "true");
   v_test_one_string (XEp_main, "/c or /b", "false");

   v_test_one_string (XEp_main, "/a and /b", "false");
   v_test_one_string (XEp_main, "/a and /*/b", "true");

   v_test_one_string (XEp_main, "18-12", "6");
   v_test_one_string (XEp_main, "18+12", "30");

   v_test_one_string (XEp_main, "count(//a|//b)", "3");
   v_test_one_string (XEp_main, "count(//*[@val])", "1");
   v_test_one_string (XEp_main, "name(//*[@val=123])", "b");

   v_test_one_string (XEp_main, "3=4", "false");
   v_test_one_string (XEp_main, "3!=4", "true");
   v_test_one_string (XEp_main, "12=12", "true");
   v_test_one_string (XEp_main, "'here is a string'='here is a string'", "true");
   v_test_one_string (XEp_main, "'here is a string'!='here is a string'", "false");

   v_test_one_string (XEp_main, "/a/b/@val", "123");
   v_test_one_string (XEp_main, "count(//*/b)", "2");
   v_test_one_string (XEp_main, "name(/*/*/*[2])", "c");
   v_test_one_string (XEp_main, "name(/*)", "a");
   v_test_one_string (XEp_main, "name(/a)", "a");
   v_test_one_string (XEp_main, "name(/a/b)", "b");
   v_test_one_string (XEp_main, "name(/*/*)", "b");
   v_test_one_string (XEp_main, "name(/a/b/c)", "c");
   v_test_one_string (XEp_main, "count(/a/b/*)", "3");
   v_test_one_string (XEp_main, "ceiling(3.5)", "4");
   v_test_one_string (XEp_main, "concat('first ','second',' third','')", "first second third");
   v_test_one_string (XEp_main, "ceiling(5)", "5");
   v_test_one_string (XEp_main, "floor(3.5)", "3");
   v_test_one_string (XEp_main, "floor(5)", "5");
   v_test_one_string (XEp_main, "string-length('try')", "3");
   v_test_one_string (XEp_main, "concat(name(/a/b[1]/*[1]),' ',name(/a/b/*[2]))", "b c");
   v_test_one_string (XEp_main, "count(/a/b/*)", "3");
   v_test_one_string (XEp_main, "count(//*)", "6");
   v_test_one_string (XEp_main, "count(//b)", "2");
   v_test_one_string (XEp_main, "contains('base','as')", "true");
   v_test_one_string (XEp_main, "contains('base','x')", "false");
   v_test_one_string (XEp_main, "not(contains('base','as'))", "false");
   v_test_one_string (XEp_main, "starts-with('blabla','bla')", "true");
   v_test_one_string (XEp_main, "starts-with('blebla','bla')", "false");
   v_test_one_string (XEp_main, "substring('12345',2,3)", "234");
   v_test_one_string (XEp_main, "substring('12345',2)", "2345");
   v_test_one_string (XEp_main, "substring('12345',2,6)", "2345");
   v_test_one_string (XEp_main, "concat('[',normalize-space('  before and   after      '),']')", "[before and after]");

   // test for u_compute_xpath_node_set

   unsigned u_nb;

   TinyXPath::xpath_processor xp_proc (XEp_main, "//*");    // build the list of all element nodes in the tree
   u_nb = xp_proc . u_compute_xpath_node_set ();            // retrieve number of nodes 
   sprintf (ca_res, "%d", u_nb);
   o_ok = (u_nb == 6);
   v_out_one_line ("//*", ca_res, "6", o_ok);

   // regression test for multiple additive expressions
   v_test_one_string (XEp_main, "2+3+4+5", "14");
   v_test_one_string (XEp_main, "20-2-3+5", "20");

   // regression test for predicate count bug
   v_test_one_string (XEp_main, "count(/a/x[1])", "1");
   v_test_one_string (XEp_main, "name(/a/*[2])", "x");
   v_test_one_string (XEp_main, "name(/a/*[1])", "b");
   v_test_one_string (XEp_main, "name(/a/x[1])", "x");

   // regression test for bug in position()
   v_test_one_string (XEp_main, "count(/a/b/c[1])", "1");
   v_test_one_string (XEp_main, "count(/a/b/c[position()=1])", "1");
   v_test_one_string (XEp_main, "count(/a/b/d[position()=3])", "0");

   // regression test for bug in i_compute_xpath
   i_res = TinyXPath::i_xpath_int (XEp_main, "//*[@val]/@val");
   sprintf (ca_res, "%d", i_res);
   v_out_one_line ("//*[@val]/@val", ca_res, "123", i_res == 123);

   // regression test for bug in text in expressions
   v_test_one_string (XEp_main, "//x[text()='sub text']/@target", "xyz");

   // regression test for bug in o_xpath_double
   o_res = TinyXPath::o_xpath_double (XEp_main, "substring('123.4',1)", d_out);            
   if (o_res)
   {
      sprintf (ca_res, "%.1f", d_out);
      o_ok = ! strcmp (ca_res, "123.4");
   }
   else
      o_ok = false;
   v_out_one_line ("substring('123.4',1)", ca_res, "123.4", o_ok);

   // testing for syntax error
   TinyXPath::xpath_processor xp_proc_2 (XEp_main, "//**");
   i_res = xp_proc_2 . i_compute_xpath ();
   if (xp_proc_2 . e_error == TinyXPath::xpath_processor::e_error_syntax)
   {
      o_ok = true;
      strcpy (ca_res, "syntax error");
   }
   else
   {
      o_ok = false;
      sprintf (ca_res, "error %d", xp_proc_2 . e_error);
   }
   v_out_one_line ("//**", ca_res, "syntax error", o_ok);

   // regression test for bug in "text" being an element
   fprintf (Fp_out_html, "</table>\n");

   delete XDp_doc;
   XDp_doc = new TiXmlDocument ();
   XDp_doc -> Parse ("<xml><text>within</text></xml>");
   XEp_main = XDp_doc -> RootElement ();
   fprintf (Fp_out_html, "<h2>Input XML tree</h2>\n");
   v_out_html (Fp_out_html, XDp_doc, 0);
   fprintf (Fp_out_html, "<br /><br />\n");
   fprintf (Fp_out_html, "<table border='1'><tr><th>Expression</th><th>Result</th><th>Expected (%s)</th></tr>\n",
      #ifdef LIBXML_CHECK
         "libXML"
      #else
         "compiled"
      #endif
         );
   v_test_one_string (XEp_main, "/xml/text/text()", "within" ); 
   

   #ifdef LIBXML_CHECK
      xmlFreeDoc (Dp_doc);
   #endif
   delete XDp_doc;

   fprintf (Fp_out_html, "</table>\n");

   fprintf (Fp_out_html, "<h2>RSS feed examples</h2>\n");
   fprintf (Fp_out_html, "These examples show how to decode a typical XML file : the <a href='http://sourceforge.net/export/rss2_projnews.php?group_id=53396&rss_fulltext=1'>TinyXPath RSS feed</a><br /><br />");

   XDp_doc = new TiXmlDocument;
   if (! XDp_doc -> LoadFile ("rss2_projnews.xml"))
   {
      printf ("Can't find rss2_projnews.xml !\n");
      return 99;
   }

   const char * cp_rss_count = "count(/rss/channel/item)";
   const char * cp_rss_ver = "/rss/@version";

   int i_nb_news, i_news;
   char ca_expr [1000];

   i_nb_news = TinyXPath::i_xpath_int (XDp_doc -> RootElement (), cp_rss_count);
   fprintf (Fp_out_html, "RSS version (XPath expr : <b>%s</b>) : %s<br />\n", 
            cp_rss_ver, TinyXPath::S_xpath_string (XDp_doc -> RootElement (), cp_rss_ver) . c_str ());
   fprintf (Fp_out_html, "Nb of news messages (XPath expr : <b>%s</b>) : %d<br />\n", 
            cp_rss_count, i_nb_news);
   fprintf (Fp_out_html, "<br /><table border='1'><tr><th>Xpath expr</th><th>value</th></tr>\n");
   for (i_news = 0; i_news < i_nb_news; i_news++)
   {
      sprintf (ca_expr, "concat(/rss/channel/item[%d]/pubDate/text(),' : ',/rss/channel/item[%d]/title/text())",
               i_news + 1, i_news + 1);
      fprintf (Fp_out_html, "<tr><td>%s</td><td>%s</td></tr>\n",
               ca_expr, TinyXPath::S_xpath_string (XDp_doc -> RootElement (), ca_expr) . c_str ());
   }
   fprintf (Fp_out_html, "</table>\n");
   delete XDp_doc;
   
   fprintf (Fp_out_html, "<br />&nbsp;<br /></body></html>\n");
   fclose (Fp_out_html);


   return 0;
}
