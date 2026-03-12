#include "../src/c12-lib.h"

struct SimpleContext {
	vec<string> Identifiers; // vec used here because string is non-trivial type
						// may cause undefined behavior if used with DynArray

	Map<string,u32> map;

	u32 intern(string text) {
		if(auto iter = map.find(text); iter != map.end()){
			// found the element
			return iter->second;
		}

		u32 id = static_cast<u32>(Identifiers.size());
		Identifiers.push_back(text);
		map[text] = id;
		return id;
	}

	string get(u32 id) const {
		return Identifiers[id];
	}

	
};



int main() {
	SimpleContext ctx;
	auto Hello = ctx.intern("Hello");
	auto World = ctx.intern("World");
	auto one= ctx.intern("one");
	auto two = ctx.intern("two");
	auto three = ctx.intern("three");
	auto four = ctx.intern("four");
	auto five = ctx.intern("five");
	auto six = ctx.intern("six");
	auto seven = ctx.intern("seven");

	DynArray<u32> list;
	list.append({Hello,World,one,two,three,four,five,six,seven});
	
	for(u32 i =0; i < list.len; i++){
	//	 std::println("{} - {} at {}",i,ctx.get(list[i]),list[i]);
	}

	 Optional<int> tmp;
	 printf("%d\n",tmp.value());	

	list.destroy();
}




















