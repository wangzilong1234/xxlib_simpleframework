#include "xx_mempool.h"
#include <iostream>

using namespace xx;

struct A;
struct B;
typedef MemPool<A, B> MP;

struct A : MPObject {};
struct B : MPObject
{
	A* a;
	B()
	{
		mempool<MP>().CreateTo(a);
	}
	~B()
	{
		a->Release();
	}
};

int main()
{
	MP mp;
	auto b = mp.Create<B>();
	b->Release();
	return 0;
}
