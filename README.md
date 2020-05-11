# Jsonic
Single header, single pass, in place JSON parser in c++

It parses a source string in a single pass without making a copy of that string, useful for quick inspection on existing data.

This is still a work in progress, with better searching and iteration being most needful.

# Example
```c++
std::string jsonString;
// ...

// Initialize root member
Jsonic::Member root(jsonString.c_str(),jsonString.length());

Jsonic::Parse(root);

// You can search or access sub-members directly

Jsonic::Member const* object   = root.Find("ObjName");
for( Jsonic::Member const& member : object->members )
{
   Jsonic::Value value	= member.GetValue();
   int num              = (value.IsNumber())? value.AsInt() : 0;
   std::string str      = (value.IsString())? value.AsString() : "";
}
```