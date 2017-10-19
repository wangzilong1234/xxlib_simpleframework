﻿#pragma execution_character_set("utf-8")
#pragma once

// todo: 各种 check stack 补足
// todo: 有空直接将序列化的代码整合过来, 该类不再继承, 配合 lua_mempool 做独立版

#include "xx_bbuffer.h"
namespace xx
{
	struct Lua_BBuffer;
	typedef Dock<Lua_BBuffer> Lua_BBuffer_v;
	struct Lua_BBuffer : BBuffer
	{
		constexpr static const char* name = "BBuffer";

		// 向 lua 映射全局的 BBuffer 表/元表
		inline static int Init(lua_State *L)
		{
			luaL_Reg funcs[] =
			{
				{ "__gc", __gc },
				{ "Create", Create },
				{ "WriteBoolean", WriteBoolean },
				{ "WriteInt8", WriteInt8 },
				{ "WriteInt16", WriteInt16 },
				{ "WriteInt32", WriteInt32 },
				{ "WriteInt64", WriteInt64 },
				{ "WriteUInt8", WriteUInt8 },
				{ "WriteUInt16", WriteUInt16 },
				{ "WriteUInt32", WriteUInt32 },
				{ "WriteUInt64", WriteUInt64 },
				{ "WriteFloat", WriteFloat },
				{ "WriteDouble", WriteDouble },
				{ "WriteString", WriteString },
				{ "WriteBBuffer", WriteBBuffer },
				{ "WriteRoot", WriteRoot },
				{ "WriteObject", WriteObject },
				//{ "WriteList", WriteList },

				{ "ReadBoolean", ReadBoolean },
				{ "ReadInt8", ReadInt8 },
				{ "ReadInt16", ReadInt16 },
				{ "ReadInt32", ReadInt32 },
				{ "ReadInt64", ReadInt64 },
				{ "ReadUInt8", ReadUInt8 },
				{ "ReadUInt16", ReadUInt16 },
				{ "ReadUInt32", ReadUInt32 },
				{ "ReadUInt64", ReadUInt64 },
				{ "ReadFloat", ReadFloat },
				{ "ReadDouble", ReadDouble },
				{ "ReadString", ReadString },
				{ "ReadBBuffer", ReadBBuffer },
				{ "ReadRoot", ReadRoot },
				//{ "ReadObject", ReadObject },
				//{ "ReadList", ReadList },

				//{ "WriteUInt8Zero", WriteUInt8Zero },
				//{ "WriteTypeId", WriteTypeId },
				//{ "ReadTypeId", ReadTypeId },

				//{ "BeginWrite", BeginWrite },
				//{ "EndWrite", EndWrite },
				//{ "WriteOffset", WriteOffset },

				//{ "BeginRead", BeginRead },
				//{ "EndRead", EndRead },
				//{ "ReadOffset", ReadOffset },

				{ "GetDataLen", GetDataLen },
				{ "GetOffset", GetOffset },
				{ "Dump", Dump },
				// todo: write package ?

				{ nullptr, nullptr }
			};
			lua_createtable(L, 0, _countof(funcs));		// mt
			luaL_setfuncs(L, funcs, 0);					// mt
			lua_pushvalue(L, -1);					    // mt, mt
			lua_setfield(L, -2, "__index");				// mt
			lua_pushlightuserdata(L, 0);				// mt, lud
			lua_setfield(L, -2, "null");				// mt			用 lud: 0 来代表空值占位符的元素
			lua_setglobal(L, name);						// 
			return 0;
		}

		inline static MemPool* GetMP(lua_State* L)
		{
			MemPool* mp;
			lua_getallocf(L, (void**)&mp);
			return mp;
		}


		inline static int __gc(lua_State* L)
		{
			auto& self = *(Lua_BBuffer_v*)lua_touserdata(L, -1);
			self.~Lua_BBuffer_v();
			self->ReleasePtrDict(L);					// 异常: 因为是移除, 应该不会抛
			self->ReleaseIdxDict(L);
			return 0;
		}

		inline static int Create(lua_State* L)
		{
			auto& self = *(Lua_BBuffer_v*)lua_newuserdata(L, sizeof(Lua_BBuffer_v));	//	异常: 无副作用
			auto mp = GetMP(L);							// ud
			lua_getglobal(L, name);						// ud, mt							异常: 未执行构造函数, 不需要析构
			new (&self) Lua_BBuffer_v(*mp);				//									异常: 因为未 bind 元表, 不会执行到析构
			lua_setmetatable(L, -2);					// ud
			return 1;
		}

		inline static Lua_BBuffer_v& GetSelf(lua_State* L, int top)
		{
			if (lua_gettop(L) != top)
			{
				luaL_error(L, "bad args nums");
			}
			auto selfptr = (Lua_BBuffer_v*)lua_touserdata(L, 1);
			if (!selfptr)
			{
				luaL_error(L, "first arg isn't BBuffer( forget \":\" ? )");
			}
			return *selfptr;
		}

		inline static int WriteBoolean(lua_State* L)
		{
			auto& self = GetSelf(L, 2);
			if (!lua_isboolean(L, 2))
			{
				luaL_error(L, "the arg's type must be a bool");
			}
			self->Write(lua_toboolean(L, 2) != 0);
			return 0;
		}

		template<typename T>
		inline static int WriteNum(lua_State* L)
		{
			static_assert(std::is_arithmetic_v<T>);
			auto& self = GetSelf(L, 2);
			int isnum;
			T v;
			if constexpr(!std::is_floating_point_v<T>)
			{
				v = (T)lua_tointegerx(L, 2, &isnum);
			}
			else
			{
				v = (T)lua_tonumberx(L, 2, &isnum);
			}
			if (!isnum)
			{
				luaL_error(L, "the arg's type must be a integer / number");
			}
			self->Write(v);
			return 0;
		}

		inline static int WriteInt8(lua_State* L) { return WriteNum<int8_t>(L); }
		inline static int WriteInt16(lua_State* L) { return WriteNum<int16_t>(L); }
		inline static int WriteInt32(lua_State* L) { return WriteNum<int32_t>(L); }
		inline static int WriteInt64(lua_State* L) { return WriteNum<int64_t>(L); }
		inline static int WriteUInt8(lua_State* L) { return WriteNum<uint8_t>(L); }
		inline static int WriteUInt16(lua_State* L) { return WriteNum<uint16_t>(L); }
		inline static int WriteUInt32(lua_State* L) { return WriteNum<uint32_t>(L); }
		inline static int WriteUInt64(lua_State* L) { return WriteNum<uint64_t>(L); }
		inline static int WriteFloat(lua_State* L) { return WriteNum<float>(L); }
		inline static int WriteDouble(lua_State* L) { return WriteNum<double>(L); }

		inline static int GetDataLen(lua_State* L)
		{
			auto& self = GetSelf(L, 1);
			lua_pushinteger(L, self->dataLen);
			return 1;
		}

		inline static int GetOffset(lua_State* L)
		{
			auto& self = GetSelf(L, 1);
			lua_pushinteger(L, self->offset);
			return 1;
		}


		// nil 写入 0, 非 nil 写入 typeId(1) + 长度 + 内容
		inline static int WriteString(lua_State* L)
		{
			auto& self = GetSelf(L, 2);
			if (lua_isnil(L, 2))
			{
				self->Write((uint8_t)0);
				return 0;
			}
			if (!lua_isstring(L, 2))
			{
				luaL_error(L, "the arg's type must be a string / nil");
			}
			size_t dataLen;
			auto v = lua_tolstring(L, 2, &dataLen);
			self->Reserve(self->dataLen + 6 + (uint32_t)dataLen);
			self->Write((uint8_t)1);
			self->Write(dataLen);
			if (dataLen)
			{
				memcpy(self->buf + self->dataLen, v, dataLen);
				self->dataLen += (uint32_t)dataLen;
			}
			return 0;
		}

		// nil 写入 0, 非 nil 写入 typeId(2) + 长度 + 内容
		inline static int WriteBBuffer(lua_State* L)
		{
			auto& self = GetSelf(L, 2);
			if (lua_isnil(L, 2))
			{
				self->Write((uint8_t)0);
				return 0;
			}
			if (!lua_isuserdata(L, 2))	// 这里先这样简单检查. 暂不检查其是否具备特定元表
			{
				luaL_error(L, "the arg's type must be a BBuffer / nil");
			}

			auto& bb = *(Lua_BBuffer_v*)lua_touserdata(L, 2);
			self->Write((uint8_t)2);
			bb->ToBBuffer(*self);
			return 0;
		}

		inline static int Dump(lua_State* L)
		{
			auto& self = GetSelf(L, 1);
			xx::String_v s(self->mempool());
			self->ToString(s.instance);
			lua_pushlstring(L, s->C_str(), s->dataLen);
			return 1;
		}


		inline static int ReadBoolean(lua_State* L)
		{
			auto& self = GetSelf(L, 1);
			bool v;
			if (self->Read(v))
			{
				luaL_error(L, "read bool error!");
			}
			lua_pushboolean(L, v);
			return 1;
		}

		template<typename T>
		inline static int ReadNum(lua_State* L)
		{
			static_assert(std::is_arithmetic_v<T>);
			auto& self = GetSelf(L, 1);
			T v;
			if (self->Read(v))
			{
				luaL_error(L, "read num error!");
			}
			if constexpr(std::is_integral_v<T>)
			{
				lua_pushinteger(L, (lua_Integer)v);
			}
			else
			{
				lua_pushnumber(L, (lua_Number)v);
			}
			return 1;
		}

		inline static int ReadInt8(lua_State* L) { return ReadNum<int8_t>(L); }
		inline static int ReadInt16(lua_State* L) { return  ReadNum<int16_t>(L); }
		inline static int ReadInt32(lua_State* L) { return  ReadNum<int32_t>(L); }
		inline static int ReadInt64(lua_State* L) { return  ReadNum<int64_t>(L); }
		inline static int ReadUInt8(lua_State* L) { return  ReadNum<uint8_t>(L); }
		inline static int ReadUInt16(lua_State* L) { return ReadNum<uint16_t>(L); }
		inline static int ReadUInt32(lua_State* L) { return ReadNum<uint32_t>(L); }
		inline static int ReadUInt64(lua_State* L) { return ReadNum<uint64_t>(L); }
		inline static int ReadFloat(lua_State* L) { return  ReadNum<float>(L); }
		inline static int ReadDouble(lua_State* L) { return ReadNum<double>(L); }

		inline static int ReadString(lua_State* L)
		{
			auto& self = GetSelf(L, 1);
			uint8_t typeId;
			if (self->Read(typeId))
			{
				luaL_error(L, "read string typeId error");
			}
			if (typeId)
			{
				if (typeId != 1)
				{
					luaL_error(L, "read string typeId error: typeId != 1");
				}
				xx::String_v s(self->mempool());
				if (self->Read(s))
				{
					luaL_error(L, "read string content error");
				}
				lua_pushlstring(L, s->C_str(), s->dataLen);
			}
			else
			{
				lua_pushnil(L);
			}
			return 1;
		}

		inline static int ReadBBuffer(lua_State* L)
		{
			auto& self = GetSelf(L, 1);
			uint8_t typeId;
			if (self->Read(typeId))
			{
				luaL_error(L, "read BBuffer typeId error");
			}
			if (typeId)
			{
				if (typeId != 2)
				{
					luaL_error(L, "read BBuffer typeId error: typeId != 2");
				}
				Create(L);											// self, bb
				auto& bb = *(Lua_BBuffer_v*)lua_touserdata(L, 2);
				if (bb->FromBBuffer(*self))
				{
					lua_pop(L, 1);									// self
					luaL_error(L, "read string content error");
				}
			}
			else
			{
				lua_pushnil(L);
			}
			return 1;
		}


		//inline static int BeginRead(lua_State* L)
		//{
		//	auto& self = GetSelf(L, 1);
		//	self->offsetRoot = self->offset;
		//	self->CreateIdxDict(L);
		//	return 0;
		//}

		//inline static int EndRead(lua_State* L)
		//{
		//	auto& self = GetSelf(L, 1);
		//	self->ReleaseIdxDict(L);
		//	return 0;
		//}


		//// 针对 string, bbuffer, table, 根据当前 offset - offsetRoot 的值, 判断是否已经读入过, 返回先前 或 新建的对象. 
		//// 需要传入对象的创建函数, 第二参数非 nil 表示该对象为新建, 需要填充
		//inline static int ReadOffset(lua_State* L)
		//{
		//	auto& self = GetSelf(L, 2);
		//	uint32_t ptr_offset = 0, bb_offset_bak = self->offset - self->offsetRoot;
		//	if (self->Read(ptr_offset)) return luaL_error(L, "read offset error.");
		//	if (ptr_offset == bb_offset_bak)
		//	{
		//		// 这里先不做严格检查了. 如果需要做, 则此处需要将 lua 的检查函数注册到 bb 才行
		//		lua_pushvalue(L, 2);								// bb, f, f
		//		lua_call(L, 0, 1);									// bb, f, rtv
		//		self->PushIdxDict(L);								// bb, f, rtv, dict
		//		lua_pushinteger(L, ptr_offset);						// bb, f, rtv, dict, offset
		//		lua_pushvalue(L, 3);								// bb, f, rtv, dict, offset, rtv
		//		lua_rawset(L, -3);									// bb, f, rtv, dict
		//		return 2;											// 第二个参数非 nil
		//	}
		//	else
		//	{
		//		self->PushIdxDict(L);								// bb, f, dict
		//		lua_pushinteger(L, ptr_offset);						// bb, f, dict, offset
		//		lua_rawget(L, -2);									// bb, f, dict, rtv
		//		lua_remove(L, 3);									// bb, f, rtv
		//		return 1;											// 第二个参数为 nil
		//	}
		//}




		inline void CreatePtrDict(lua_State* L)
		{
			lua_pushlightuserdata(L, this);				// ud, &ud
			lua_createtable(L, 0, 64);					// ud, &ud, t
			lua_rawset(L, LUA_REGISTRYINDEX);			// ud
		}
		inline void CreateIdxDict(lua_State* L)
		{
			lua_pushlightuserdata(L, (char*)this + 1);	// ud, &ud
			lua_createtable(L, 0, 64);					// ud, &ud, t
			lua_rawset(L, LUA_REGISTRYINDEX);			// ud
		}

		inline void ReleasePtrDict(lua_State* L)
		{
			lua_pushlightuserdata(L, this);				// &ud
			lua_pushnil(L);								// &ud, nil
			lua_rawset(L, LUA_REGISTRYINDEX);			//
		}
		inline void ReleaseIdxDict(lua_State* L)
		{
			lua_pushlightuserdata(L, (char*)this + 1);	// &ud
			lua_pushnil(L);								// &ud, nil
			lua_rawset(L, LUA_REGISTRYINDEX);			//
		}

		inline void PushPtrDict(lua_State* L)
		{
			lua_pushlightuserdata(L, this);				// ... key
			lua_rawget(L, LUA_REGISTRYINDEX);			// ... table
		}
		inline void PushIdxDict(lua_State* L)
		{
			lua_pushlightuserdata(L, (char*)this + 1);	// ... key
			lua_rawget(L, LUA_REGISTRYINDEX);			// ... table
		}


		// 针对 string, bbuffer, table, 写 dataLen - offsetRoot 到 buf, 返回是否第一次写入
		inline bool WriteOffset(lua_State* L)
		{
			switch (lua_type(L, -1))							    // ..., o
			{
			case LUA_TSTRING:
			case LUA_TTABLE:
			case LUA_TUSERDATA:
				PushPtrDict(L);										// ..., o, dict					定位到注册表中的 ptrDict
				lua_pushvalue(L, -2);								// ..., o, dict, o				查询当前对象是否已经记录过
				lua_rawget(L, -2);									// ..., o, dict, nil/offset
				if (lua_isnil(L, -1))								// 如果未记录则记录 + 写buf
				{
					auto offset = dataLen - offsetRoot;
					Write(offset);									// 写buf

					lua_pop(L, 1);									// ..., o, dict
					lua_pushvalue(L, -2);							// ..., o, dict, o
					lua_pushinteger(L, offset);						// ..., o, dict, o, offset
					lua_rawset(L, -3);								// ..., o, dict
					lua_pop(L, 1);									// ..., o
					return true;									// 返回首次出现
				}
				else												// 记录过则取其 value 来写buf
				{
					auto offset = (uint32_t)lua_tointeger(L, -1);
					Write(offset);									// 写buf
					lua_pop(L, 2);									// ..., o
					return false;
				}
			default:
				luaL_error(L, "the arg's type must be a string / table / BBuffer");
				return false;
			}
		}

		inline static int WriteObject(lua_State* L)
		{
			auto& self = GetSelf(L, 2);				// bb, o
			self->WriteObject_(L);					// bb, o
			return 0;
		}


		// 写对象到 buf. 如果为空就写 typeId 0, 否则写 typeId, offset[, content ]
		inline void WriteObject_(lua_State* L)
		{
			if (lua_isnil(L, -1) ||					// bb, o
				lua_islightuserdata(L, -1) && (size_t)lua_touserdata(L, -1) == 0)
			{
				Write((uint8_t)0);
			}
			else
			{
				lua_pushstring(L, "__proto");		// bb, o, "__proto"
				lua_rawget(L, -2);					// bb, o, proto
				lua_pushstring(L, "typeId");		// bb, o, proto, "typeId"
				lua_rawget(L, -2);					// bb, o, proto, num
				auto typeId = (uint16_t)lua_tonumber(L, -1);
				Write(typeId);
				lua_pushvalue(L, -3);				// bb, o, proto, num, o
				if (WriteOffset(L))					// bb, o, proto, num, o
				{
					lua_pop(L, 2);					// bb, o, proto
					lua_pushstring(L, "ToBBuffer");	// bb, o, proto, "ToBBuffer"
					lua_rawget(L, -2);				// bb, o, proto, func
					lua_pushvalue(L, 1);			// bb, o, proto, func, bb
					lua_pushvalue(L, -4);			// bb, o, proto, func, bb, o
					lua_call(L, 2, 0);				// bb, o, proto
					lua_pop(L, 1);					// bb, o
				}
			}
		}

		inline static int WriteRoot(lua_State* L)
		{
			auto& self = GetSelf(L, 2);				// bb, o
			self->offsetRoot = self->dataLen;
			self->CreatePtrDict(L);

			self->WriteObject_(L);					// bb, o

			self->ReleasePtrDict(L);
			return 0;
		}







		inline static int ReadRoot(lua_State* L)
		{
			// todo
			return 1;
		}

	};
};
