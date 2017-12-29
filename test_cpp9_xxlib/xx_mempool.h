﻿#pragma once
namespace xx
{
	/***********************************************************************************/
	// 公共内存头相关
	/***********************************************************************************/

	// Alloc 出来的都带这个头
	struct MemHeader
	{
		union
		{
			uint64_t versionNumber;		// 用于野指针判定
			uint8_t vnArea[8];
		};
		uint8_t& mpIndex() { return vnArea[7]; }	// 内存池数组 下标
	};

	// Create 出来的都带这个头
	struct MemHeader_Object : MemHeader
	{
		union
		{
			uint64_t objectData;		// 方便清 0
			struct
			{
				// 受 Ptr 类影响, Ptr 构造时 +1, 析构时 -1, 减到 0 时 Release
				uint32_t refs;

				// MemPool 创建时填充类型ID
				uint16_t typeId;

				// 当前用于 ToString 防重入( 或是存空格缩进值 )
				uint16_t flags;
			};
		};
	};


	/***********************************************************************************/
	// TypeId 映射
	/***********************************************************************************/

	template<typename T>
	struct TypeId
	{
		static const uint16_t value = 0;
	};

	template<typename T>
	constexpr uint16_t TypeId_v = TypeId<T>::value;


	// 一些预声明

	template<typename T>
	class Ptr;

	template<typename T>
	class Ref;

	class BBuffer;
	class Object;
	class String;


	/***********************************************************************************/
	// 粗暴快速的非线程安全内存池
	/***********************************************************************************/

	class MemPool
	{
		static_assert(sizeof(size_t) <= sizeof(void*));

		// 数组长度涵盖所有 2^n 对齐分配长度规则
		std::array<void*, sizeof(void*) * 8> headers;

		// 循环自增版本号( 每次创建时先 ++ 再填充 )
		decltype(MemHeader::versionNumber) versionNumber = 0;

	public:
		MemPool() noexcept;
		~MemPool() noexcept;
		MemPool(MemPool const&) = delete;
		MemPool& operator=(MemPool const&) = delete;

		template<typename MHType = MemHeader>
		void* Alloc(size_t siz) noexcept;

		template<typename MHType = MemHeader>
		void Free(void* p) noexcept;

		// 可用于创建 lua state
		void* Realloc(void *p, size_t newSize, size_t dataLen = -1) noexcept;


		template<typename T, typename ...Args>
		Ptr<T> Create(Args &&... args);

		template<typename T, typename ...Args>
		bool CreateTo(Ptr<T>& outPtr, Args &&... args);


		/***********************************************************************************/
		// 工具函数

		static size_t Calc2n(size_t n) noexcept;

		// 将长度值按 2^n 来对齐
		static size_t Round2n(size_t n) noexcept;


		/***********************************************************************************/
		// typeId & 序列化 相关

		// 放置 type 对应的 parent 的 type id. 1 : Object
		inline static std::array<uint16_t, std::numeric_limits<uint16_t>::max()> pids;

		typedef void*(*creator)(MemPool*, BBuffer*, size_t);

		// 存 typeId 到序列化构造函数的映射
		inline static std::array<creator, 1 << (sizeof(uint16_t) * 8)> creators;

		// 注册类型的父子关系. 顺便生成创建函数. Object 不需要注册. T 需要提供相应构造函数 for 反序列化
		template<typename T, typename PT>
		static void Register() noexcept;

		// 根据 typeid 判断父子关系
		static bool IsBaseOf(uint32_t baseTypeId, uint32_t typeId) noexcept;

		// 根据 类型 判断父子关系
		template<typename BT>
		static bool IsBaseOf(uint32_t typeId) noexcept;

		// 根据 类型 判断父子关系
		template<typename BT, typename T>
		static bool IsBaseOf() noexcept;

		// 试将指针 p 转成 T* 类型. 取代 dynamic_cast
		template<typename T>
		static T* TryCast(Object* p) noexcept;

	};




	/***********************************************************************************/
	// 所有非 MemPool 类的基类
	/***********************************************************************************/

	class Object
	{
	public:
		MemPool * mempool;
		Object(MemPool* mempool) noexcept;
		Object(BBuffer* bb) noexcept;
		virtual ~Object() noexcept;

		MemHeader_Object& memHeader() noexcept;
		MemHeader_Object& memHeader() const noexcept;

		virtual void ToString(String &s) const;

		virtual void ToBBuffer(BBuffer &bb) const;
		virtual int FromBBuffer(BBuffer &bb);
	};




	/***********************************************************************************/
	// std::shared_ptr like
	/***********************************************************************************/

	template<typename T>
	class Ptr
	{
	public:
		static_assert(std::is_base_of_v<Object, T>);
		typedef T ChildType;
		T* pointer;

		Ptr() noexcept;

		template<typename O>
		Ptr(O* const& pointer) noexcept;

		template<typename O>
		Ptr& operator=(O* const& o) noexcept;


		template<typename O>
		Ptr(Ptr<O> const& o) noexcept;

		template<typename O>
		Ptr& operator=(Ptr<O> const& o) noexcept;


		template<typename O>
		Ptr(Ptr<O>&& o) noexcept;

		template<typename O>
		Ptr& operator=(Ptr<O>&& o) noexcept;

		void Release();
		~Ptr();

		template<typename O>
		bool operator==(Ptr<O> const& o) const noexcept
		{
			return pointer == o.pointer;
		}


		operator bool() const noexcept;
		// unsafe funcs
		T const* operator->() const noexcept;
		T* & operator->() noexcept;
		T& operator*() noexcept;
		T const& operator*() const noexcept;
	};

	template<typename T>
	struct IsPtr
	{
		static const bool value = false;
	};

	template<typename T>
	struct IsPtr<Ptr<T>>
	{
		static const bool value = true;
	};

	template<typename T>
	constexpr bool IsPtr_v = IsPtr<T>::value;


	typedef Ptr<Object> Object_p;




	/***********************************************************************************/
	// shared_ptr like
	/***********************************************************************************/

	template<typename T>
	class Ref
	{
	public:
		typedef T ChildType;
		T* pointer;
		decltype(MemHeader::versionNumber) versionNumber;

		Ref() noexcept
			: pointer(nullptr)
			, versionNumber(0)
		{}

		template<typename O>
		Ref(Ptr<O> const& o) noexcept
			: pointer(o.pointer)
		{
			static_assert(std::is_base_of_v<T, O>);
			versionNumber = o ? o->memHeader().versionNumber : 0;
		}

		template<typename O>
		Ref& operator=(Ptr<O> const& o) noexcept
		{
			static_assert(std::is_base_of_v<T, O>);
			pointer = o.pointer;
			versionNumber = o ? o->memHeader().versionNumber : 0;
			return *this;
		}

		template<typename O = T>
		Ptr<O> Ptr() const noexcept
		{
			static_assert(std::is_base_of_v<O, T>);
			if (!pointer || pointer->memHeader().versionNumber == versionNumber)
			{
				return Ptr<O>();
			}
			return Ptr<O>(pointer);
		}

		operator bool() const noexcept
		{
			return !pointer || pointer->memHeader().versionNumber == versionNumber;
		}

		template<typename O>
		Ref(Ref<O> const& o) noexcept
		{
			operator=(o.Ptr());
		}

		template<typename O>
		Ref& operator=(Ref<O> const& o) noexcept
		{
			return operator=(o.Ptr());
		}

		//template<typename O>
		//bool operator==(Ptr<O> const& o) const noexcept
		//{
		//	return Ptr() == o.Ptr();
		//}

		//template<typename O>
		//bool operator==(Ref<O> const& o) const noexcept
		//{
		//	return pointer == o.pointer;
		//}

	};


	template<typename T>
	struct IsRef
	{
		static const bool value = false;
	};

	template<typename T>
	struct IsRef<Ref<T>>
	{
		static const bool value = true;
	};

	template<typename T>
	constexpr bool IsRef_v = IsRef<T>::value;

	typedef Ref<Object> Object_r;

}