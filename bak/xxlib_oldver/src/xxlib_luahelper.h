#pragma once
#include <cassert>
#include <tuple>
#include <type_traits>
#include <utility>
#include <array>
#include <initializer_list>
#include "xxlib_mpobject.h"
#include "lua.hpp"

namespace xxlib
{
	// todo: 改进宏, 默认向路由类传入全局量 luaBinder. 类里面再用下面这两行来得到 tuple. 最后用 LuaTypeIndex 来推算 type 的 index

	//typedef typename decltype(luaBinder)::Tuple LuaBinderTuple;

	//template<typename T>
	//using LuaTypeIndex = xxlib::TupleIndexOf<T, LuaBinderTuple>;


	/*

	struct A {};
	struct B :A {};
	struct C :B {};
	struct D {};

	xxlib::LuaBinder<A, B, C, D> luaBinder;

	*/

	template<typename ... Types>
	struct LuaBinder
	{
		typedef std::tuple<MPObject, Types...> Tuple;
		std::array<uint16_t, 1 + sizeof...(Types)> pids;	// 0: MPObject

		LuaBinder()
		{
			Fill(std::make_index_sequence<std::tuple_size<Tuple>::value>());
		}

		// 根据 类型序号 判断父子关系
		bool IsBaseOf(uint16_t baseTypeId, uint16_t typeId)
		{
			assert(baseTypeId < std::tuple_size<Tuple>::value && typeId < std::tuple_size<Tuple>::value);
			for (; typeId != baseTypeId; typeId = pids[typeId])
			{
				if (!typeId) return false;
			}
			return true;
		}

		// 根据 类型 判断父子关系
		template<typename BT, typename T>
		bool IsBaseOf()
		{
			return IsBaseOf(TupleIndexOf<BT, Tuple>::value, TupleIndexOf<T, Tuple>::value);
		}

		// 批量创建出所有元表放置到 L( 一直占据 stack 空间 )
		void CreateMetatables(lua_State* L)
		{
			assert(0 == lua_gettop(L));
			for (int i = 0; i < std::tuple_size<Tuple>::value; ++i)
			{
				CreateMetatable(L, i);
			}
		}

		// 遇到 T*, SceneObjRef<T> 类型要 TryTo 或 核实 self 时, 用这个函数来检测合法性.
		template<typename T>
		inline bool LuaCheck(lua_State* L, int idx)
		{
			static_assert(TupleIndexOf<T, Tuple>::value, "T is not in Types.");
			if (!lua_getmetatable(L, idx)) return false;	// ... mt?
			bool b = false;
			if (lua_rawgeti(L, -1, 1) == LUA_TNUMBER)		// ... mt, tidx?
			{
				b = IsBaseOf((uint16_t)lua_tointeger(L, -1), TupleIndexOf<T, Tuple>::value);
			}
			lua_pop(L, 2);									// ...
			return b;
		}

	protected:
		void CreateMetatable(lua_State* L, int idx)
		{
			lua_newtable(L);									// t

																// 写入类型索引
			lua_pushinteger(L, idx);							// t, idx
			lua_rawseti(L, -2, 1);								// t

																// 填充 __index
			lua_pushstring(L, "__index");						// t, k
			lua_pushvalue(L, -2);								// t, k, t
			lua_rawset(L, -3);									// t
		}

		template<size_t... Indexs, typename T>
		void Fill(std::index_sequence<Indexs...> const& idxs, T* const& ct)
		{
			uint16_t pid = 0;
			std::initializer_list<int>{ ((
				std::is_base_of<std::tuple_element_t<Indexs, Tuple>, T>::value && !std::is_same<std::tuple_element_t<Indexs, Tuple>, T>::value
				? (pid = TupleIndexOf<std::tuple_element_t<Indexs, Tuple>, Tuple>::value) : 0
				), 0)... };
			pids[TupleIndexOf<T, Tuple>::value] = pid;
		}

		template<size_t... Indexs>
		void Fill(std::index_sequence<Indexs...> const& idxs)
		{
			std::initializer_list<int>{ (Fill(idxs, (std::tuple_element_t<Indexs, Tuple>*)nullptr), 0)... };
		}

		//// 判断 目标类型 是否从基类指针派生( 某基类指针是否能转换成目标类型 ) 以取代 dynamic_cast
		//template<typename T>
		//bool IsInheritOf(MPObject* base)
		//{
		//	return IsBaseOf(base->typeId(), TupleIndexOf<T, Tuple>::value);
		//}
	};

	/*****************s*******************************************************************/
	// Lua_Container / Lua_CreatContainer
	/************************************************************************************/

	// 在此设置全局容器名
	constexpr auto Lua_Container = "objs";

	// 创建全局容器
	inline void Lua_CreatContainer(lua_State* L)
	{
		lua_newtable(L);						// t
		lua_setglobal(L, Lua_Container);		//
	}


	/************************************************************************************/
	// Lua_Metatable
	/************************************************************************************/

	// 基础适配模板
	// 用来填充类型与元表的 下标 的映射关系
	template<typename T>
	struct Lua_Metatable;

	/************************************************************************************/
	// LuaCheck
	/************************************************************************************/

	// 遇到 T, T*, SceneObjRef<T> 类型要 TryTo 或 核实 self 时, 用这个函数来检测合法性.
	// 要求 ud/lud 具备 mt, 且 mt[1] 要等于 Lua_Metatable<T>::index
	template<typename T>
	inline bool LuaCheck(lua_State* L, int idx)
	{
		auto rtv = lua_getmetatable(L, idx);			// ... mt?
		if (!rtv) return false;
		// todo: 检查 mt 是否为 T 或其 Parent 的 mt
		lua_pop(L, 1);
		return true;
	}


	/************************************************************************************/
	// LuaError
	/************************************************************************************/

	// 简化出错函数调用. 用法: return LuaError("...");
	inline int LuaError(lua_State* L, char const* errmsg)
	{
		lua_pushstring(L, errmsg);
		lua_error(L);
		return 0;
	}

	/************************************************************************************/
	// LuaSelf<T>::Get
	/************************************************************************************/

	// 将 self 转为 T* 的形态返回. 为空则表示转换失败或指针变野
	// 根据 lua_islightuserdata 结合 is_base_of<MPObject, T> 来判断到底是哪种情况
	template<typename T, typename ENABLE = void>
	struct LuaSelf;

	template<typename T>
	struct LuaSelf<T, std::enable_if_t< std::is_base_of<MPObject, T>::value>>
	{
		static T* Get(lua_State* L)
		{
			if (lua_islightuserdata(L, 1))
			{
				return (T*)lua_touserdata(L, 1);
			}
			auto& ptr = *(SceneObjRef<T>*)lua_touserdata(L, 1);
			if (ptr) return ptr.pointer;
			return nullptr;
		}
	};
	template<typename T>
	struct LuaSelf<T, std::enable_if_t<!std::is_base_of<MPObject, T>::value>>
	{
		static T* Get(lua_State* L)
		{
			return (T*)lua_touserdata(L, 1);
		}
	};


	/************************************************************************************/
	// Lua<T, cond>
	/************************************************************************************/

	// 封装了一组根据数据类型来路由的 Push / To 函数


	template<typename T, typename ENABLE = void>
	struct Lua
	{
		static void PushUp_Mt(lua_State* L);
		static void Push(lua_State* L, std::string const& v);
		static void To(lua_State* L, std::string& v, int idx);
		static bool TryTo(lua_State* L, std::string& v, int idx);
	};

	// void
	template<>
	struct Lua<void, void>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}
	};

	// string
	template<>
	struct Lua<std::string, void>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}

		static inline void Push(lua_State* L, std::string const& v)
		{
			lua_pushlstring(L, v.data(), v.size());
		}
		static inline void To(lua_State* L, std::string& v, int idx)
		{
			size_t len;
			auto s = lua_tolstring(L, idx, &len);
			v.assign(s, len);
		}
		static inline bool TryTo(lua_State* L, std::string& v, int idx)
		{
			if (!lua_isstring(L, idx)) return false;
			To(L, v, idx);
			return true;
		}
	};

	// char*
	template<>
	struct Lua<char const*, void>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}

		static inline void Push(lua_State* L, char const* const& v)
		{
			lua_pushstring(L, v);
		}
		static inline void To(lua_State* L, char const*& v, int idx)
		{
			v = lua_tostring(L, idx);
		}
		static inline bool TryTo(lua_State* L, char const*& v, int idx)
		{
			if (!lua_isstring(L, idx)) return false;
			v = lua_tostring(L, idx);
			return true;
		}
	};

	// fp
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_floating_point<T>::value>>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			lua_pushnumber(L, (lua_Number)v);
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = (T)lua_tonumber(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_isnumber(L, idx)) return false;
			v = (T)lua_tonumber(L, idx);
			return true;
		}
	};

	// int
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushnil(L);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			lua_pushinteger(L, (lua_Integer)v);
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = (T)lua_tointeger(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_isinteger(L, idx)) return false;
			v = (T)lua_tointeger(L, idx);
			return true;
		}
	};

	// T*
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_pointer<T>::value>>
	{
		typedef std::remove_pointer_t<T> TT;

		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushvalue(L, Lua_Metatable<TT>::index);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			lua_pushlightuserdata(L, v);								// lud
			lua_getupvalue(L, -lua_gettop(L) - 1, 1);					// lud, mt
			lua_setmetatable(L, -2);									// lud
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = (T)lua_touserdata(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_islightuserdata(L, idx)) return false;
			if (!LuaCheck<TT>(L, idx)) return false;
			v = (T)lua_touserdata(L, idx);
			return true;
		}
	};

	// T
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_class<T>::value && !IsMPObject<T>::value>>
	{
		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushvalue(L, Lua_Metatable<T>::index);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			auto p = (T*)lua_newuserdata(L, sizeof(v));					// ud
			(*p) = v;
			lua_getupvalue(L, -lua_gettop(L) - 1, 1);					// ud, mt
			lua_setmetatable(L, -2);									// ud
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = *(T*)lua_touserdata(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_isuserdata(L, idx)) return false;
			if (!LuaCheck<T>(L, idx)) return false;
			v = *(T*)lua_touserdata(L, idx);
			return true;
		}
	};

	// SceneObjRef<T>
	template<typename T>
	struct Lua<T, std::enable_if_t<std::is_class<T>::value && IsMPObject<T>::value>>
	{
		typedef typename T::type TT;

		static inline void PushUp_Mt(lua_State* L)
		{
			lua_pushvalue(L, Lua_Metatable<TT>::index);
		}

		static inline void Push(lua_State* L, T const& v)
		{
			auto p = (T*)lua_newuserdata(L, sizeof(v));					// ud
			(*p) = v;
			lua_getupvalue(L, -lua_gettop(L) - 1, 1);					// ud, mt
			lua_setmetatable(L, -2);									// ud
		}
		static inline void To(lua_State* L, T& v, int idx)
		{
			v = *(T*)lua_touserdata(L, idx);
		}
		static inline bool TryTo(lua_State* L, T& v, int idx)
		{
			if (!lua_isuserdata(L, idx)) return false;
			if (!LuaCheck<TT>(L, idx)) return false;
			v = *(T*)lua_touserdata(L, idx);
			return true;
		}
	};

	// todo: more 适配 
	// todo: std::vector<T>
	// todo: std::vector<T*>
	// todo: std::vector<SceneObjRef<T>>


	/************************************************************************************/
	// Lua_PushMetatable, Lua_CopyItemsFromParentMetatable
	/************************************************************************************/

	// 创建一个基础 metatable 到栈. __index 执行方式为查表, 且元表指向自身
	template<typename T>
	inline void Lua_PushMetatable(lua_State* L, int numFieldsCapacity, int numMethodsCapacity)
	{
		lua_createtable(L, numFieldsCapacity, numMethodsCapacity);	// t
		assert(xxlib::Lua_Metatable<T>::index == lua_gettop(L));
		// 写入位于 L 中的下标
		auto top = lua_gettop(L);
		lua_pushinteger(L, top);							// t, idx
		lua_rawseti(L, -2, 1);								// t
															// 填充 __index
		lua_pushstring(L, "__index");						// t, k
		lua_pushvalue(L, -2);								// t, k, t
		lua_rawset(L, -3);									// t
	}




	// 从指定 index 处 clone 元素到 -1 表
	template<typename Parent>
	inline void Lua_CopyItemsFromParentMetatable(lua_State* L)
	{
		lua_pushnil(L);
		while (lua_next(L, Lua_Metatable<Parent>::index) != 0)
		{
			lua_pushvalue(L, -2);
			lua_insert(L, -2);
			lua_rawset(L, -4);
		}
	}


	/************************************************************************************/
	// Lua_CreateCoroutine
	/************************************************************************************/

	// 为状态机( 基类是 MPObject )建协程放至 LUA_REGISTRYINDEX 并返回指针
	// 主要供状态机构造函数处调用. o 传入 this
	inline lua_State* Lua_CreateCoroutine(lua_State* L, void* key)
	{
		auto co = lua_newthread(L);							// key, co
		lua_rawsetp(L, LUA_REGISTRYINDEX, key);				//
		return co;
	}


	/************************************************************************************/
	// Lua_ReleaseCoroutine
	/************************************************************************************/

	// 移除一个协程 ( 位于 LUA_REGISTRYINDEX 表 )
	inline void Lua_ReleaseCoroutine(lua_State* L, void* key)
	{
		if (!L) return;										// 如果是 scene 析构( L 已死 )就会导致这个情况
		lua_pushnil(L);
		lua_rawsetp(L, LUA_REGISTRYINDEX, key);
	}

	/************************************************************************************/
	// Lua_CreateSceneObjRef
	/************************************************************************************/

	// todo: 往 co 直接传值( 当前方案相当于是会创建出2份实例来 )

	// 为 MPObject 建 SceneObjRef<T> userdata 放至全局容器( key 为 pureVersionNumber )
	// 主要供含有 LUA 协程状态机 的 MPObject 之构造函数处调用. o 传入 this
	template<typename T>
	inline void Lua_CreateSceneObjRef(lua_State* L, T* o)
	{
		static_assert(std::is_base_of<MPObject, T>::value, "the T must be inherit of MPObject");
		auto rtv = lua_getglobal(L, Lua_Container);			// objs
		assert(rtv == LUA_TTABLE);
		auto p = lua_newuserdata(L, sizeof(SceneObjRef<T>));// objs, ud
		new (p) SceneObjRef<T>(o);
		lua_pushvalue(L, Lua_Metatable<T>::index);			// objs, ud, mt
		lua_setmetatable(L, -2);
		lua_rawseti(L, -2, o->pureVersionNumber());			// objs
		lua_pop(L, 1);										//
	}


	/************************************************************************************/
	// Lua_ReleaseSceneObjRef
	/************************************************************************************/

	// 从全局容器杀协程
	// 主要供状态机析构函数处调用. o 传入 this
	inline void Lua_ReleaseSceneObjRef(lua_State* L, MPObject* o)
	{
		if (!L) return;										// 如果是 scene 析构( L 已死 )就会导致这个情况
		auto rtv = lua_getglobal(L, Lua_Container);			// objs
		assert(rtv == LUA_TTABLE);
		lua_pushnil(L);										// objs, nil
		lua_rawseti(L, -2, o->pureVersionNumber());			// objs
		lua_pop(L, 1);										//
	}


	/************************************************************************************/
	// Lua_CreatePointerToGlobal
	/************************************************************************************/

	// 为指针类型创建 lightuserdata 并放到全局 _G( 主要用于全局单例如 SceneManager 之类 )
	template<typename T>
	inline void Lua_CreatePointerToGlobal(lua_State* L, T* o, char const* name)
	{
		lua_pushlightuserdata(L, o);						// lud
		lua_pushvalue(L, Lua_Metatable<T>::index);			// lud, mt
		lua_setmetatable(L, -2);							// lud
		lua_setglobal(L, name);
	}


	/************************************************************************************/
	// Lua_Resume
	/************************************************************************************/

	inline int Lua_Resume(lua_State* co, std::string& err)
	{
		assert(co);
		int status = lua_resume(co, nullptr, 0);
		if (status == LUA_YIELD)
		{
			return 0;	// todo: 暂存函数的返回值? 
		}
		else if (status == LUA_ERRRUN && lua_isstring(co, -1))
		{
			err = lua_tostring(co, -1);
			lua_pop(co, -1);
			return -1;
		}
		else
		{
			return 1;	// LUA_OK
		}
	}




	/************************************************************************************/
	// Lua_TupleFiller
	/************************************************************************************/

	template<typename Tuple, std::size_t N>
	struct Lua_TupleFiller
	{
		static bool Fill(lua_State* L, Tuple& t)
		{
			auto rtv = Lua<std::tuple_element_t<N - 1, Tuple>>::TryTo(L, std::get<N - 1>(t), -(int)(std::tuple_size<Tuple>::value - N + 1));
			if (!rtv) return false;
			return Lua_TupleFiller<Tuple, N - 1>::Fill(L, t);
		}
	};
	template<typename Tuple>
	struct Lua_TupleFiller < Tuple, 1 >
	{
		static bool Fill(lua_State* L, Tuple& t)
		{
			return Lua<std::tuple_element_t<0, Tuple>>::TryTo(L, std::get<0>(t), -(int)(std::tuple_size<Tuple>::value));
		}
	};

	template<typename Tuple>
	bool Lua_FillTuple(lua_State* L, Tuple& t)
	{
		return Lua_TupleFiller<Tuple, std::tuple_size<Tuple>::value>::Fill(L, t);
	}

	/************************************************************************************/
	// Lua_CallFunc
	/************************************************************************************/

	// todo: linux 下测试通过

	// 已知问题: 标记为 YIELD 方式执行的函数, 将忽略直接返回值.
	// 有参数 有返回值
	template<bool YIELD, typename T, typename R, typename ...Args>
	int Lua_CallFunc(std::enable_if_t<sizeof...(Args) && !std::is_void<R>::value, lua_State*> L, T* o, R(T::* f)(Args...))
	{
		std::tuple<Args...> t;
		if (Lua_FillTuple(L, t))
		{
			auto rtv = FuncTupleCaller(o, f, t, std::make_index_sequence<sizeof...(Args)>());
			if (YIELD) return lua_yield(L, 0);
			Lua<R>::Push(L, rtv);
			return 1;
		}
		return 0;
	}
	// 无参数 有返回值
	template<bool YIELD, typename T, typename R, typename ...Args>
	int Lua_CallFunc(std::enable_if_t<!sizeof...(Args) && !std::is_void<R>::value, lua_State*> L, T* o, R(T::* f)(Args...))
	{
		auto rtv = (o->*f)();
		if (YIELD) return lua_yield(L, 0);
		Lua<R>::Push(L, rtv);
		return 1;
	}
	// 有参数 无返回值
	template<bool YIELD, typename T, typename R, typename ...Args>
	int Lua_CallFunc(std::enable_if_t<sizeof...(Args) && std::is_void<R>::value, lua_State*> L, T* o, R(T::* f)(Args...))
	{
		std::tuple<Args...> t;
		if (Lua_FillTuple(L, t))
		{
			FuncTupleCaller(o, f, t, std::make_index_sequence<sizeof...(Args)>());
			if (YIELD) return lua_yield(L, 0);
			else return 0;
		}
		return 0;
	}
	// 无参数 无返回值
	template<bool YIELD, typename T, typename R, typename ...Args>
	int Lua_CallFunc(std::enable_if_t<!sizeof...(Args) && std::is_void<R>::value, lua_State*> L, T* o, R(T::* f)(Args...))
	{
		(o->*f)();
		if (YIELD) return lua_yield(L, 0);
		else return 0;
	}


	/************************************************************************************/
	// Lua_BindFunc_Ensure
	/************************************************************************************/

	template<typename T>
	int Lua_BindFunc_Ensure_Impl(lua_State* L)
	{
		auto tn = typeid(T).name();
		auto top = lua_gettop(L);
		if (top != 1 || !LuaCheck<T>(L, 1))
		{
			lua_pushliteral(L, "error!!! func args num wrong.");
			lua_error(L);
			return 0;
		}
		T* self = LuaSelf<T>::Get(L);
		lua_pushboolean(L, self ? true : false);
		return 1;
	}

	// 生成确认 ud 是否合法的 Ensure 指令
	template<typename T>
	inline void Lua_BindFunc_Ensure(lua_State* L)
	{
		static_assert(std::is_base_of<MPObject, T>::value, "only MPObject* struct have Ensure func.");
		lua_pushstring(L, "Ensure");
		lua_pushcfunction(L, Lua_BindFunc_Ensure_Impl<T>);
		lua_rawset(L, -3);
	}

}



/************************************************************************************/
// xxLua_BindXXXXXXXXX
/************************************************************************************/

#define xxLua_BindField(LUA, T, F, writeable)									\
lua_pushstring(LUA, #F);														\
xxlib::Lua<decltype(xxlib::GetFieldType(&T::F))>::PushUp_Mt(LUA);				\
lua_pushcclosure(LUA, [](lua_State* L)											\
{																				\
	auto top = lua_gettop(L);													\
	if (!top || !xxlib::LuaCheck<T>(L, 1))										\
	{																			\
		return xxlib::LuaError(L, "error!!! : ? bad self type?");				\
	}																			\
	T* self = xxlib::LuaSelf<T>::Get(L);										\
	if (!self)																	\
	{																			\
		return xxlib::LuaError(L, "error!!! self is nil!");						\
	}																			\
	if (top == 1)																\
	{																			\
		xxlib::Lua<decltype(self->F)>::Push(L, self->F);				 		\
		return 1;																\
	}																			\
	if (top == 2)																\
	{																			\
		if (!writeable)															\
		{																		\
			return xxlib::LuaError(L, "error!!! readonly!");					\
		}																		\
		else if (!xxlib::Lua<decltype(self->F)>::TryTo(L, self->F, 2))			\
		{																		\
			return xxlib::LuaError(L, "error!!! bad data type!");				\
		}																		\
		return 0;																\
	}																			\
	return xxlib::LuaError(L, "error!!! too many args!");						\
}, 1);																			\
lua_rawset(LUA, -3);


#define xxLua_BindFunc(LUA, T, F, YIELD)										\
lua_pushstring(LUA, #F);														\
xxlib::Lua<decltype(xxlib::GetFuncReturnType(&T::F))>::PushUp_Mt(LUA);			\
lua_pushcclosure(LUA, [](lua_State* L)											\
{																				\
	auto top = lua_gettop(L);													\
	auto numArgs = xxlib::GetFuncArgsCount(&T::F);								\
	if (top != numArgs + 1 || !xxlib::LuaCheck<T>(L, 1))						\
	{																			\
		return xxlib::LuaError(L, "error!!! wrong num args or bad self type.");	\
	}																			\
	T* self = xxlib::LuaSelf<T>::Get(L);										\
	if (!self)																	\
	{																			\
		return xxlib::LuaError(L, "error!!! self is nil!");						\
	}																			\
	return xxlib::Lua_CallFunc<YIELD>(L, self, &T::F);							\
}, 1);																			\
lua_rawset(LUA, -3);

