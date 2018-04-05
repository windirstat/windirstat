#ifndef __STDMACRO_H__
#define __STDMACRO_H__
// determine number of elements in an array (not bytes)
#ifndef _countof
#	define _countof(array) (sizeof(array)/sizeof(array[0]))
#endif
// 2^n
#ifndef	twoto
#	define twoto(x)	(1<<(x))
#endif
// BIT OPERATION
#define BITINC(f,v)		((f)&(v))
#define BITINK(f,v)		(((f)&(v))==(v))
#define BITADD(f,v)		(f)|=(v);
#define BITREM(f,v)		(f)&=~(v);
#define BITMOD(f,a,r)	(f)=((f)&(~(r)))|(a);
#define BITSWT(f,v)		if(BITINC(f,v)){BITREM(f,v)}else{BITADD(f,v)}
#define BITIIF(f,v,b)	if((b)){BITADD(f,v)}else{BITREM(f,v)}
/*** STRING ***/
#define ASCII2NUM(c)	(ISDIGIT(c)?(c-'0'):(TOUPPER(c)-'A'+10))
#define ISASCII(c)		(AMID(c,0,127))
#define	ISSPACE(c)		((c)=='\r'||(c)=='\n'||(c)=='\v'||(c)=='\t'||(c)==' ')
#define	ISUPPER(c)		(AMID(c,'A'/*65*/,'Z'/*90*/))
#define	ISLOWER(c)		(AMID(c,'a'/*97*/,'z'/*122*/))
#define	ISDIGIT(c)		(AMID(c,'0'/*48*/,'9'/*57*/))
#define	ISXDIGIT(c)		(ISDIGIT(c)||AMID(c,'A'/*65*/,'F'/*70*/)||AMID(c,'a'/*97*/,'f'/*102*/))
#define	ISALPHA(c)		(ISUPPER(c)||ISLOWER(c))
#define	ISALNUM(c)		(ISALPHA(c)||ISDIGIT(c))
#define ISBDIGIT(c)		('0'/*48*/==(c)||'1'/*49*/==(c))
#define ISODIGIT(c)		(AMID(c,'0'/*48*/,'7'/*55*/))
#define	ISCNTRL(c)		(AMID(c,/*(NUL)*/0,/*(US)*/0x1F)||/*(DEL)*/0x7F==(c))

#define	ISBLANK(c)		(/*32*/' '==(c))
#define	ISPUNCT(c)		(ISPRINT(c) && !ISALNUM(c) && !ISSPACE(c))

#define	ISGRAPH(c)		(ISPUNCT(c)||ISALNUM(c))
#define	ISPRINT(c)		(ISBLANK(c)||ISGRAPH(c))

#define ISNAMECHR1(c)	(ISALPHA(c)||'_'/*95*/==(c))
#define ISNAMECHR(c)	(ISALNUM(c)||'_'/*95*/==(c))
#define ISSLASH(c)		((c)=='/'||(c)=='\\')
#define ISPAREN(c)		((c)=='('||(c)==')')
//
#define TOLOWER(c)		(ISUPPER(c)?((c)+32):(c))
#define TOUPPER(c)		(ISLOWER(c)?((c)-32):(c))
/*** MISC ***/
//CASTING
#ifndef cast
#	define cast(t, exp)	((t)(exp))
#endif
// is between
#define AMID(c,x,y)		(((x)<=(c))&&((c)<=(y)))
// avoid comp err
#define UNUSED(x) (void)(x)
// Ternary ops
#define IIF(b,t,f)		((b)?(t):(f))
// swap 2 numbers
#define SWAP(a,b)		((a)^=(b)^=(a)^=(b))
// for to loop
#define FORTO(v,x,y)	for((v)=(x);(v)<(y);(v)++)
// string equal
#define STREQ(s1,s2)	(strcmp((const char *)(s1),(const char *)(s2))==0)
#define STRIEQ(s1,s2)	(strncmp((const char *)(s1),(const char *)(s2))==0)
// string not equal
#define STRNEQ(s1,s2)	(strcmp((const char *)(s1),(const char *)(s2))!=0)

#define	DZTS_ITER_INIT(T,V,P) {T V = (P);while(V[0]){
#define	DZTS_ITER_CONT(V)	  while((V++)[0]!=0);}}

// iter std::list l = std::list; a & b = std::list iters
#define StdContainer_EraseAllItem(c)	(c.erase(c.begin(),c.end()))
#define StdContainer_ForEachItem(l,a,o)	for(a=l.begin(),o=l.end();a!=o;a++)
#define StdContainer_RemoveItem(l,a,n)	(n=a;n++,l.erase(a),a=n)

#endif //__STDMACRO_H__