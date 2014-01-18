bin_PROGRAMS = tinyxpath

tinyxpath_SOURCES = htmlutil.cpp main.cpp \
   tinystr.cpp tinyxml.cpp tinyxmlerror.cpp tinyxmlparser.cpp \
   action_store.cpp lex_util.cpp node_set.cpp tokenlist.cpp xml_util.cpp \
   xpath_expression.cpp xpath_processor.cpp xpath_stack.cpp xpath_stream.cpp xpath_syntax.cpp xpath_static.cpp

tinyxpath_LDADD = 

# set the include path found by configure
INCLUDES= $(all_includes) 

AM_CXXFLAGS = -Wall

# the library search path.
tinyxpath_LDFLAGS = $(all_libraries)
