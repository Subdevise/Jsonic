
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



#pragma once

#ifndef _JSONIC_INCLUDED
#define _JSONIC_INCLUDED

#include <vector>
#include <functional>


namespace Jsonic
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
		Value(char const* sz,size_t szLen,size_t memLen=0) : type(ValueString),num(0),len(szLen)
		{
			if( len>0 )
			{
				str	= (char*)malloc(memLen>len ? memLen : len+1);
				memcpy(str,sz,len);	str[len] = '\0';
			}
		}
		Value(Value const& a) : Value()	{ operator=(a); }
		Value(Value&& a) : Value() { operator=((Value&&)a); }	// Must cast, or copy version is called
		~Value() { if( len>0 ) free(str); }	// Use len and not str to check (str is in union)

		Value& operator=(Value const& a)
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
				if( a.len>0 )
				{
					if( len==0 )
					{
						str	= (char*)malloc(len+1);
					}
					len	= a.len;
					memcpy(str,a.str,len);	str[len] = '\0';
				}
				else
				{
					num	= a.num;
				}
			}
			return *this;
		}
		Value& operator=(Value&& a)
		{
			type	= a.type;
			len		= a.len;
			// Copy largest union member
			num		= a.num;
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
		size_t		len;
		union
		{
			char*	str;
			double	num;
			bool	boolean;
		};
	};


	//
	// Helper function to parse the inside of JSON quotes
	// Checks for escape characters
	//
	inline Value ParseString(char const* sz,size_t len)
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
				if( i >= len-1 )	return Value(ValueError);
				ci	= sz[i+1];
				if( ci=='b' )		ci	= '\b';
				else if( ci=='f' )	ci	= '\f';
				else if( ci=='n' )	ci	= '\n';
				else if( ci=='r' )	ci	= '\r';
				else if( ci=='t' )	ci	= '\t';
				else if( ci=='u' )
				{
					if( i >= len-5 )	return Value(ValueError);
					hex[0]	= sz[i+2];	hex[1]	= sz[i+3];	hex[2]	= sz[i+4];	hex[3]	= sz[i+5];
					int num	= strtol(hex,nullptr,16);
					num		= WideToMB(hex,(wchar_t)num);
					for( int k=0; k<num-1; ++k )
					{
						*out++	= hex[k];
						ci		= hex[k+1];
					}
				}
			}
			*out++	= ci;
		}
		v.len	= out - v.str;
		return v;
	}



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
		Value GetValue() const
		{
			if( str==nullptr || len==0 )
			{
				return Value(ValueNull);
			}
			// Iterator
			size_t szLen	= len;
			char const* sz	= Trim(str,&szLen);

			if( *str == '\"' )
			{
				if( len < 2 )
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
			s32 nTrue	= 0;
			s32 nFalse	= 0;
			s32 nNull	= 0;
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

		//
		// Returns the value of the named member. Will not recursively search.
		//
		Value GetValue(char const* sz,size_t szLen=0) const
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
						return (i<members.size()-1)? members[i+1].GetValue() : Value(ValueNull);
					}
				}
			}
			return Value(ValueNull);
		}


		//
		// Searches keys recursively to find the specified member
		//
		Member const* Find(char const* sz,size_t szLen=0) const
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
					Member const* p;
					if( (p=m.Find(sz,szLen)) != nullptr )	return p;
				}
			}
			return nullptr;
		}

		MemberType			type;
		char const*			str;
		size_t				len;
		std::vector<Member>	members;
	};



	//
	// Pass in a root member with .str and .len set to valid values
	// members will contain pointers to the original string (no strings are allocated)
	//
	inline bool Parse(Member& root)
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
	
		while( *psz!='\0' )
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


};


#endif