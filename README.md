# Jsonic
Single header, single pass, in place JSON parser in c++

It parses a source string in a single pass without making a copy. String data is only parsed/copied when the value is requested through Value::AsString().

Define JSONIC_IMPLEMENTATION before including in a single source file.

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

   Jsonic::Member const* child = member.Find("child_member");
   if( child != nullptr )
   {
      double f = child->GetValue().AsDouble();
   }
}
```

You can also build JSON data using the BuildNode structure. The overloaded BuildNode constructor can be used to create values, arrays, and objectsa and can be added to other BuildNodes recursively.

# JSON construction Example
```c++

BuildNode root;
BuildNode item;
item.AddNode("width", BuildNode((int)100));
item.AddNode("height", BuildNode((int)200));
// Add item to the root node
root.AddNode("item", item);

std::vector<int> items;
for( size_t i=0; i<24; ++i )
{
   items.push_back(i);
}
// Add an array of integers to the root node
root.AddNode("list", BuildNode(items));
```