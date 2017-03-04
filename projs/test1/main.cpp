#include "xx_luahelper.h"
#include <iostream>

// types defines
/***********************************************************************/

struct A;
struct B;
struct C;
struct Factor
{
	A* CreateA();
	B* CreateB();
	C* CreateC();
	C CreateStructC();
	void NeedA(A* a);
	void NeedB(B* b);
	void NeedC(C* c);
};
struct A {};
struct B : A {};
struct C : B {};

// MP defines
/***********************************************************************/

typedef xx::MemPool<A, B, C, Factor> MP;

// impls
/***********************************************************************/

A* Factor::CreateA()
{
	std::cout << "CreateA" << std::endl;
	return nullptr;
}
B* Factor::CreateB()
{
	std::cout << "CreateB" << std::endl;
	return nullptr;
}
C* Factor::CreateC()
{
	std::cout << "CreateC" << std::endl;
	return nullptr;
}
C Factor::CreateStructC()
{
	std::cout << "CreateStructC" << std::endl;
	return C();
}
void Factor::NeedA(A* a)
{
	std::cout << "NeedA" << std::endl;
}
void Factor::NeedB(B* b)
{
	std::cout << "NeedB" << std::endl;
}
void Factor::NeedC(C* c)
{
	std::cout << "NeedC" << std::endl;
}

// main
/***********************************************************************/

int main()
{
	MP mp;
	auto L = xx::Lua_NewState(mp);

	xx::Lua_PushMetatable<MP, Factor>(L);
	xxLua_BindFunc(MP, L, Factor, CreateA, false);
	xxLua_BindFunc(MP, L, Factor, CreateB, false);
	xxLua_BindFunc(MP, L, Factor, CreateC, false);
	xxLua_BindFunc(MP, L, Factor, CreateStructC, false);
	xxLua_BindFunc(MP, L, Factor, NeedA, false);
	xxLua_BindFunc(MP, L, Factor, NeedB, false);
	xxLua_BindFunc(MP, L, Factor, NeedC, false);
	lua_pop(L, 1);

	xx::Lua_SetGlobal<MP>(L, "factor", Factor());
	auto rtv = luaL_dostring(L, R"%%(

local pointer_a = factor:CreateA()
factor:NeedC( pointer_a )

)%%");
	if (rtv)
	{
		std::cout << lua_tostring(L, -1) << std::endl;
	}

	lua_close(L);
	std::cin.get();
	return 0;
}
