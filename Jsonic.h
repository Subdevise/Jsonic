
/*
 * Copyright (c) 2020 Subdevise
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#ifndef _JSONIC_INCLUDED
#define _JSONIC_INCLUDED

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <ctype.h>


namespace jsonic
{

	enum MemberType
	{
		OBJECT=0,
		KEY,
		ARRAY,
		VALUE
	};

	enum ValueType
	{
		ValueNull=0,
		ValueString,
		ValueNumber,
		ValueBoolean,
		ValueError
	};


	struct Value
	{
		Value() : type(ValueNull),num(0),len(0) {}
		Value(ValueType t) : type(t),num(0),len(0) {}
		Value(double val) : type(ValueNumber),num(val),len(0) {}
		Value(bool val) : type(ValueBoolean),boolean(val),len(0) {}
		Value(char const* sz,size_t szLen,size_t memLen=0) : type(ValueString),num(0),len(szLen),zero('\0')
		{
			if( len>0 )
			{
				str	= (char*)malloc(memLen>len ? memLen : len+1);
				memcpy(str,sz,len);	str[len] = '\0';
			}
			else
			{
				str	= &zero;
			}
		}
		Value(Value const& a) : Value()	{ operator=(a); }
		Value(Value&& a) : Value() { operator=((Value&&)a); }	// Must cast, or copy version is called
		~Value() { if( len>0 ) free(str); }	// Use len and not str to check (str is in union)

		inline Value& operator=(Value const& a)
		{
			if( this!=&a )
			{
				type	= a.type;
				// Free memory if 'a' does not have a string, or is a longer string
				if( len>0 && (len < a.len || a.len==0) )
				{
					len	= 0;
					free(str);
				}
				if( type == ValueString )
				{
					if( a.len>0 )
					{
						if( len==0 )
						{
							str	= (char*)malloc(a.len + 1);
						}
						len	= a.len;
						memcpy(str,a.str,len);	str[len] = '\0';
					}
					else
					{
						zero	= '\0';
						str		= &zero;
					}
				}
				else
				{
					num	= a.num;
				}
			}
			return *this;
		}
		inline Value& operator=(Value&& a)
		{
			type	= a.type;
			len		= a.len;
			str		= a.str;
			a.len	= 0;
			a.str	= nullptr;
			return *this;
		}

		ValueType GetType() const		{ return type; }
		bool IsNull() const				{ return type==ValueNull; }
		bool IsNumber() const			{ return type==ValueNumber; }
		bool IsString() const			{ return type==ValueString; }
		bool IsBoolean() const			{ return type==ValueBoolean; }

		char const* AsString() const	{ return str; }
		double		AsDouble() const	{ return num; }
		int			AsInt() const		{ return (int)num; }
		bool		AsBoolean() const	{ return boolean; }

		ValueType	type;
		char		zero;
		size_t		len;
		union
		{
			char*	str;
			double	num;
			bool	boolean;
		};
	};


	//
	// Member is the container for any JSON token - Object, Key, Array and Value
	// The 'str' and 'len' point to the information in the source string
	// Use GetValue to convert that information to a value - Null, String, Number, Boolean or Error
	//
	struct Member
	{
		Member() : type(OBJECT) {}
		Member(char const* sz,size_t szLen) : type(OBJECT),str(sz),len(szLen) {}
		Member(Member const& a) { *this = a; }
		Member& operator=(Member const& a)
		{
			if( this!=&a )
			{
				str		= a.str;
				len		= a.len;
				type	= a.type;
				members	= a.members;
			}
			return *this;
		}
		~Member() {}

		// Performs transformations (escape characters, removing quotes, convert to num etc.)
		Value GetValue() const;
		bool GetKey(std::string& str) const;

		// Find a child member
		Member const* Find(char const* sz,size_t szLen=0) const;
		Member const* FindRecursive(char const* sz,size_t szLen) const;

		struct V2
		{
			char const* str;
			size_t		len;
		};

		MemberType			type;
		char const*			str;
		size_t				len;
		std::vector<Member>	members;
		std::vector<V2>		values;	// single/array of strings, numbers, bool (empty is null)
	};


	//
	// Pass in a root member with .str and .len set to valid values
	// members will contain pointers to the original string (no strings are allocated)
	//
	bool Parse(Member& root);


	//
	//
	//
	class BuildNode
	{
		public:
		static void PrintNode(BuildNode const& node, std::string& json);
		
		BuildNode() : type(MemberType::OBJECT) {}
		BuildNode(std::string const& key, BuildNode const& value)
		{
			type	= MemberType::KEY;
			mKey	= key;
			nodes.push_back(value);
		}
		BuildNode(std::string const& value) : type(MemberType::VALUE)
		{
			values.push_back(ParseValue(value));
		}
		BuildNode(int value) : type(MemberType::VALUE)
		{
			values.push_back(std::to_string(value));
		}
		BuildNode(double value) : type(MemberType::VALUE)
		{
			values.push_back(std::to_string(value));
		}
		BuildNode(bool value) : type(MemberType::VALUE)
		{
			values.push_back(value ? "true" : "false");
		}
		BuildNode(nullptr_t) : type(MemberType::VALUE)
		{
			values.push_back("null");
		}
		BuildNode(std::vector<BuildNode> const& list) : type(MemberType::ARRAY)
		{
			nodes = list;
		}
		BuildNode(std::vector<std::string> const& list) : type(MemberType::ARRAY)
		{
			for( std::string const& v : list )
			{
				values.push_back(ParseValue(v));
			}
		}
		BuildNode(std::vector<int> const& list) : type(MemberType::ARRAY)
		{
			for( int v : list )
			{
				values.push_back(std::to_string(v));
			}
		}
		BuildNode(std::vector<double> const& list) : type(MemberType::ARRAY)
		{
			for( double v : list )
			{
				values.push_back(std::to_string(v));
			}
		}
		BuildNode(std::initializer_list<BuildNode> const& list) : type(MemberType::ARRAY)
		{
			nodes = list;
		}
		BuildNode(std::initializer_list<char const*> const& list) : type(MemberType::ARRAY)
		{
			for( char const* v : list )
			{
				values.push_back(ParseValue(v));
			}
		}
		BuildNode(std::initializer_list<std::string> const& list) : type(MemberType::ARRAY)
		{
			for( std::string const& v : list )
			{
				values.push_back(ParseValue(v));
			}
		}
		BuildNode(std::initializer_list<int> const& list) : type(MemberType::ARRAY)
		{
			for( int v : list )
			{
				values.push_back(std::to_string(v));
			}
		}
		BuildNode(std::initializer_list<double> const& list) : type(MemberType::ARRAY)
		{
			for( double v : list )
			{
				values.push_back(std::to_string(v));
			}
		}

		void AddNode(std::string const& key, BuildNode const& value)
		{
			nodes.push_back(BuildNode(key, value));
		}

		private:
		std::string ParseValue(std::string const& str) const
		{
			std::string result;
			result	+= '\"';
			for( char ch : str )
			{
				if( ch == '\\' )		{ result += "\\\\"; }
				else if( ch == '/' )	{ result += "\\/"; }
				else if( ch == '\"' )	{ result += "\\\""; }
				else if( ch == '\b' )	{ result += "\\b"; }
				else if( ch == '\f' )	{ result += "\\f"; }
				else if( ch == '\n' )	{ result += "\\n"; }
				else if( ch == '\r' )	{ result += "\\r"; }
				else if( ch == '\t' )	{ result += "\\t"; }
				else					{ result += ch;	}
			}
			result += '\"';
			return result;
		}
		MemberType					type;
		std::string					mKey;
		std::vector<std::string>	values;
		std::vector<BuildNode>		nodes;
	};
	
};


#ifdef JSONIC_IMPLEMENTATION

namespace jsonic
{
	// String utility functions would be useful in their own header, however, in keeping with a single header library...
	
	//
	// Returns the number of bytes required to encode the character
	//
	inline int UTF8CharLength(char ch)
	{
		uint8_t const c	= (uint8_t)ch;
		if( c <= 127 )				return 1;
		if( (c & 0xe0) == 0xc0 )	return 2;
		if( (c & 0xf0) == 0xe0 )	return 3;
		if( (c & 0xf8) == 0xf0 )	return 4;
		return 1;
	}
	inline int UTF32toUTF8Length(uint32_t ch)
	{
		constexpr uint32_t maxLegalUTF32 = 0x0010FFFF;
		if( ch < (uint32_t)0x80 )		return 1;
		if( ch < (uint32_t)0x800 )		return 2;
		if( ch < (uint32_t)0x10000)		return 3;
		if( ch <= maxLegalUTF32 )		return 4;
		return 0;
	}
	
	//
	// Convers a single unicode character and returns the number of bytes used to encode it
	//
	inline int UTF8toUTF32Char(char const* utf8, uint32_t* utf32)
	{
		char const* src = utf8;
		uint32_t c	= (uint32_t)(*src++);
		if( (c & 0x80) != 0 )
		{
			if( (c & 0xe0) == 0xc0 )
			{
				c	= (c & 0x1f) << 6;
				if( (*src & 0xc0) != 0x80 ) return 0;
				c	|= ((uint32_t)(*src++) & 0x3f);
			}
			else if( (c & 0xf0) == 0xe0 )
			{
				c	= (c & 0xf) << 12;
				if( (*src & 0xc0) != 0x80 ) return 0;
				c	|= ((uint32_t)(*src++) & 0x3f) << 6;
				if( (*src & 0xc0) != 0x80 ) return 0;
				c	|= ((uint32_t)(*src++) & 0x3f);
			}
			else if( (c & 0xf8) == 0xf0 )
			{
				c	= (c & 0x7) << 18;
				if( (*src & 0xc0) != 0x80 ) return 0;
				c	|= ((uint32_t)(*src++) & 0x3f) << 12;
				if( (*src & 0xc0) != 0x80 ) return 0;
				c	|= ((uint32_t)(*src++) & 0x3f) << 6;
				if( (*src & 0xc0) != 0x80 ) return 0;
				c	|= ((uint32_t)(*src++) & 0x3f);
			}
			else
			{
				return 0;
			}
		}
		*utf32	= c;
		return (int)(src - utf8);
	}

	//
	//
	//
	inline size_t UTF8toUTF32String(char const* utf8, uint32_t* utf32, size_t* errorPos = nullptr)
	{
		if( !utf8 || !utf32 )
		{
			if( errorPos ) *errorPos = 0;
			return 0;
		}
		char const* src = utf8;
		uint32_t* dst	= utf32;
		size_t error	= 0;

		while( *src != 0 )
		{
			int n = UTF8toUTF32Char(src, dst);
			if( n == 0 )
			{
				if( errorPos )
				{
					*errorPos = src - utf8;
				}
				return 0;
			}
			src += n;
			++dst;
		}
		*dst = 0;
		return dst - utf32;
	}

	//
	// utf8 must have a maximum of 4 bytes available
	//
	inline int UTF32toUTF8Char(uint32_t utf32, char* utf8)
	{
		static const uint8_t firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
		static const uint8_t byteMask = 0xBF;
		static const uint8_t byteMark = 0x80; 

		int const n	= UTF32toUTF8Length(utf32);
		utf8	+= n;
		switch( n )
		{
			// All cases fall through
			case 4: *--utf8 = (char)((utf32 | byteMark) & byteMask); utf32 >>= 6;
			case 3: *--utf8 = (char)((utf32 | byteMark) & byteMask); utf32 >>= 6;
			case 2: *--utf8 = (char)((utf32 | byteMark) & byteMask); utf32 >>= 6;
			case 1: *--utf8 = (char) (utf32 | firstByteMark[n]);
		}
		return n;
	}


	inline bool IsWhiteSpace(uint32_t code)
	{
		switch(code)
		{
			case 0x0009:
			case 0x000A:
			case 0x000B:
			case 0x000C:
			case 0x000D:
			case 0x0020:
			case 0x0085:
			case 0x00A0:
			case 0x1680:
			case 0x2000:
			case 0x2001:
			case 0x2002:
			case 0x2003:
			case 0x2004:
			case 0x2005:
			case 0x2006:
			case 0x2007:
			case 0x2008:
			case 0x2009:
			case 0x200A:
			case 0x2028:
			case 0x2029:
			case 0x202F:
			case 0x205F:
			case 0x2060:
			case 0x3000:
			case 0xFEFF:
				return true;
		};
		return false;
	}
	inline bool IsBreakSpace(uint32_t code)
	{
		switch(code)
		{
			case 0x0009:
			case 0x000A:
			case 0x000B:
			case 0x000C:
			case 0x000D:
			case 0x0020:
			case 0x0085:
			//case 0x00A0:
			case 0x1680:
			case 0x2000:
			case 0x2001:
			case 0x2002:
			case 0x2003:
			case 0x2004:
			case 0x2005:
			case 0x2006:
			//case 0x2007:
			case 0x2008:
			case 0x2009:
			case 0x200A:
			case 0x2028:
			case 0x2029:
			//case 0x202F:
			case 0x205F:
			//case 0x2060:
			case 0x3000:
			//case 0xFEFF:
				return true;
		};
		return false;
	}
	inline bool IsLineBreak(uint32_t code)
	{
		return code == 0x0A || code == 0x85;
	}


	inline bool IsWhiteSpace(char ch)
	{
		return IsWhiteSpace((uint32_t)ch);
	}
	inline bool IsWhiteSpace(wchar_t ch)
	{
		return IsWhiteSpace((uint32_t)ch);
	}
	inline bool IsWhiteSpace(char const* str)
	{
		uint32_t code;
		UTF8toUTF32Char(str, &code);
		return IsWhiteSpace(code);
	}

	template<class T>
	inline int TStringCmp(T const* szA,T const* szB)
	{
		while(*szA != 0)
		{
			if(*szB == 0)	return  1;
			if(*szB > *szA)	return -1;
			if(*szA > *szB)	return  1;

			szA++;
			szB++;
		}

		if(*szB != 0)	return -1;
		return 0;
	}



	//
	//
	//
	inline bool IsAscii(char ch)
	{
		return ((uint8_t)ch & 0x80) == 0;
	}
	inline bool IsUTF8(char ch)
	{
		return ((uint8_t)ch & 0xc0) != 0x80;
	}
	inline bool IsTrailingUTF8(char ch)
	{
		return ((uint8_t)ch & 0x80) != 0;
	}



	//
	//
	//
	inline size_t TrimLeft(char const* str,size_t len)
	{
		size_t i	= 0;
		while( i<len && IsWhiteSpace(&str[i]) )
		{
			i	+= UTF8CharLength(str[i]);
		}
		return i;
	}
	inline size_t TrimRight(char const* str,size_t len)
	{
		if( len==0 )	return 0;
		size_t k	= len-1;
		while( k>0 && (IsTrailingUTF8(str[k]) || IsWhiteSpace(&str[k])) )
		{
			--k;
		}
		return len-1 - k;
	}
	inline bool Trim(char const* str,size_t len,size_t* nFront,size_t* nBack)
	{
		*nFront	= TrimLeft(str,len);
		*nBack	= 0;
		if( *nFront < len )
		{
			*nBack	= TrimRight(str + *nFront,len - *nFront);
		}
		return *nFront > 0 || *nBack > 0;
	}
	inline bool Trim(char const* str,size_t len)
	{
		size_t nFront,nBack;
		return Trim(str,len,&nFront,&nBack);
	}
	inline void Trim(std::string& str)
	{
		size_t n,m;
		size_t len	= str.length();
		if( Trim(str.c_str(),len,&n,&m) )
		{
			str.erase(len - m,m);
			str.erase(0,n);
		}
	}

	//
	// Helper function to parse the inside of JSON quotes
	// Checks for escape characters
	//
	Value ParseString(char const* sz,size_t len)
	{
		char hex[8]		= {0};
		char const Esc	= '\\';

		Value v(sz,len,len*3/2);
		char* out	= v.str;

		for( size_t i=0; i<len; ++i )
		{
			char ci	= sz[i];
			if( ci == Esc )
			{
				if( i+1 >= len )	return Value(ValueError);
				ci	= sz[++i];

				// Quote("), Backslash(\), and Forward-slash(/) are already set once the escape character is removed
				if( ci=='b' )		ci	= '\b';
				else if( ci=='f' )	ci	= '\f';
				else if( ci=='n' )	ci	= '\n';
				else if( ci=='r' )	ci	= '\r';
				else if( ci=='t' )	ci	= '\t';
				else if( ci=='u' )
				{
					if( i+4 >= len )	return Value(ValueError);
					hex[0]	= sz[++i];	hex[1]	= sz[++i];	hex[2]	= sz[++i];	hex[3]	= sz[++i];
					uint32_t code	= (uint32_t)strtol(hex,nullptr,16);
					int num = UTF32toUTF8Char(code, hex);
					for( int k=0; k<num-1; ++k )
					{
						*out++	= hex[k];
					}
					ci	= hex[num-1];
				}
			}
			*out++	= ci;
		}
		v.len	= out - v.str;
		v.str[v.len] = '\0';
		return v;
	}

	Value Member::GetValue() const
	{
		if( str==nullptr || len==0 )
		{
			return Value(ValueNull);
		}
		// Iterator
		size_t nLeft,nRight;
		size_t szLen = len;
		char const* sz = str;
		if( Trim(str, len, &nLeft, &nRight) )
		{
			sz		+= nLeft;
			szLen	-= (nLeft + nRight);
		}

		if( *sz == '\"' )
		{
			if( szLen < 2 )
			{
				return Value(ValueError);
			}
			sz++;
			szLen	-= 2;
			if( *(sz+szLen) != '\"' )
			{
				// No closing quote
				return Value(ValueError);
			}
			return ParseString(sz,szLen);
		}

		static char szTrue[]	= {'t','r','u','e',0};
		static char szFalse[]	= {'f','a','l','s','e'};
		static char szNull[]	= {'n','u','l','l',0};
		int nTrue	= 0;
		int nFalse	= 0;
		int nNull	= 0;
		for( size_t i=0; i<5; ++i )
		{
			if( *(sz+i) == szTrue[i] )	nTrue++;
			if( *(sz+i) == szFalse[i] )	nFalse++;
			if( *(sz+i) == szNull[i] )	nNull++;
		}
		if( nTrue==4 )	return Value(true);
		if( nFalse==5 )	return Value(false);
		if( nNull==4 )	return Value(ValueNull);

		char* pEnd;
		errno		= 0;
		double num	= strtod(sz,&pEnd);
		if( errno == ERANGE || pEnd==str )
		{
			return Value(ValueError);
		}
		return Value(num);
	}

	Member const* Member::Find(char const* sz,size_t szLen) const
	{
		if( szLen==0 )	szLen	= strlen(sz);
		for( size_t i=0; i<members.size(); ++i )
		{
			Member const& m	= members[i];
			if( m.type==KEY )
			{
				Value v	= m.GetValue();
				if( szLen==v.len && szLen>0 && strncmp(v.AsString(),sz,szLen)==0 )
				{
					return (i<members.size()-1)? &members[i+1] : nullptr;
				}
			}
		}
		return nullptr;
	}
	Member const* Member::FindRecursive(char const* sz,size_t szLen) const
	{
		if( szLen==0 )	szLen	= strlen(sz);
		for( size_t i=0; i<members.size(); ++i )
		{
			Member const& m	= members[i];
			if( m.type==KEY )
			{
				Value v	= m.GetValue();
				if( szLen==v.len && szLen>0 && strncmp(v.AsString(),sz,szLen)==0 )
				{
					return (i<members.size()-1)? &members[i+1] : nullptr;
				}
			}
			else
			{
				Member const* p = m.Find(sz,szLen);
				if( p != nullptr )	return p;
			}
		}
		return nullptr;
	}

	bool Parse(Member& root)
	{
		static const char BlockBegin	= '{';
		static const char BlockEnd		= '}';
		static const char ArrayBegin	= '[';
		static const char ArrayEnd		= ']';
		static const char Separator		= ',';
		static const char ValueBegin	= ':';
		static const char Quote			= '\"';

		if( root.str == nullptr || root.len < 2 )
		{
			return false;
		}
	
		Member* pv		= &root;
		pv->type		= VALUE;
		char const* psz	= pv->str;

		std::vector<Member*>	stack;
		// Pseudo push the first brace (don't call PushVar, since it's not being added to a parent)
		stack.push_back(pv);

		std::function<void(MemberType)> PushVar	= [&](MemberType type)
		{
			stack.push_back(pv);
			pv->members.push_back(Member());

			pv	= &pv->members.back();
			pv->type	= type;
			// psz points to delimiter
			pv->str		= psz + 1;
			pv->len		= 0;
		};

		std::function<bool()> PopVar	= [&]()
		{
			// psz should be a delimiter, so don't include it in len
			// the exception is quotes, see Quote section in loop
			pv->len	= psz - pv->str;
			if( stack.size()>0 )
			{
				pv	= stack.back();
				stack.pop_back();
				return true;
			}
			return false;
		};
	
		while( *psz != '\0' )
		{
			if( *psz == Quote )
			{
				if( pv->type == OBJECT )
				{
					// We need to keep quotes, but discard every other delimiter
					--psz; PushVar(KEY); ++psz;
				}
				else if( pv->type != VALUE )
				{
					return false;
				}

				do
				{
					++psz;
				}while( *psz!='\0' && (*psz!=Quote || *(psz-1)=='\\') );
			
				if( pv->type == KEY )
				{
					++psz; PopVar(); --psz;
				}
			}
			else if( *psz == BlockBegin )
			{
				if( pv->type==VALUE )
				{
					pv->type	= OBJECT;
				}
				else
				{
					PushVar(OBJECT);
				}
			}
			else if( *psz == ValueBegin )
			{
				if( pv->type != OBJECT )	return false;
				PushVar(VALUE);
			}
			else if( *psz == ArrayBegin )
			{
				if( pv->type != VALUE )	return false;
				pv->type	= ARRAY;
				PushVar(VALUE);
			}
			else if( *psz == Separator )
			{
				if( pv->type == VALUE )
				{
					if( !PopVar() )	return false;
				}
				if( pv->type==ARRAY )
				{
					PushVar(VALUE);
				}
			}
			else if( *psz == ArrayEnd )
			{
				if( pv->type == VALUE )
				{
					if( !PopVar() )	return false;
				}

				if( !PopVar() )	return false;
			}
			else if( *psz == BlockEnd )
			{
				if( pv->type == VALUE )
				{
					if( !PopVar() )	return false;
				}

				if( !PopVar() )	return false;
			}
			++psz;
		}
		return stack.size()==0;
	}

	void BuildNode::PrintNode(BuildNode const& node, std::string& json)
	{
		switch( node.type )
		{
			case MemberType::OBJECT:
				json	+= '{';
				json	+= ' ';
				for( size_t i=0; i<node.nodes.size(); ++i )
				{
					PrintNode(node.nodes[i], json);
					if( i < node.nodes.size()-1 )
					{
						json	+= ',';
						json	+= ' ';
					}
				}
				json	+= ' ';
				json	+= '}';
				break;
			case MemberType::KEY:
				json	+= '\"';
				json	+= node.mKey;
				json	+= '\"';
				json	+= ':';
				json	+= ' ';
				// assert only one node and no values
				PrintNode(node.nodes.front(), json);
				break;
			case MemberType::ARRAY:
				json	+= '[';
				json	+= ' ';
				// assert either values or nodes (not both)
				for( size_t i=0; i<node.values.size(); ++i )
				{
					json	+= node.values[i];
					if( i < node.values.size()-1 )
					{
						json	+= ',';
						json	+= ' ';
					}
				}
				for( size_t i=0; i<node.nodes.size(); ++i )
				{
					PrintNode(node.nodes[i], json);
					if( i < node.nodes.size()-1 )
					{
						json	+= ',';
						json	+= ' ';
					}
				}
				json	+= ' ';
				json	+= ']';
				break;
			case MemberType::VALUE:
				// assert only one value
				json	+= node.values.front();
				break;
			default:
				// error
				break;
		};
	}
};
#endif

#endif