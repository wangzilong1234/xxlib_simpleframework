﻿// todo: noexcept 狂加一波( 部分加不了的要去掉, 该 try 还是得 try. 构造函数中的异常需要反馈给上层. Create 函数似乎不能加 noexcept )
// todo: 用 sg 语法改进各种库
// todo: xx_uv 从 c# 那边复制备注
#include <xx_uv.h>

inline xx::MemPool mp;
inline xx::BBuffer_p bbSend(mp.MPCreate<xx::BBuffer>());
inline xx::Stopwatch sw;

inline void HandlePkg(xx::UvTcpUdpBase* peer, xx::BBuffer& recvData)
{
	xx::Object_p pkg;
	if (recvData.ReadRoot(pkg) || !pkg)
	{
		peer->DisconnectImpl();
		return;
	}
	switch (pkg.GetTypeId())
	{
		case xx::TypeId_v<xx::BBuffer>:
		{
			auto& bb = pkg.As<xx::BBuffer>();
			int v = 0;
			if (bb->Read(v))
			{
				peer->DisconnectImpl();
				return;
			}
			v++;
			if (v >= 100000)
			{
				std::cout << sw() << std::endl;
				return;
			}
			bbSend->Clear();
			bbSend->Write(v);
			peer->Send(bbSend);
		}
		break;
		default:
			break;
	}
}



int main()
{
	xx::MemPool::RegisterInternals();
	xx::UvLoop loop(&mp);
	auto listener = loop.CreateTcpListener();
	listener->Bind("0.0.0.0", 12345);
	listener->OnAccept = [](auto peer)
	{
		peer->OnReceivePackage = [peer](auto& recvData)
		{
			HandlePkg(peer, recvData);
		};
	};
	listener->Listen();

	xx::UvTcpClient client(loop);
	client.OnConnect = [&](auto state)
	{
		if (client.Alive())
		{
			int v = 0;
			bbSend->Clear();
			bbSend->Write(v);
			client.Send(bbSend);
		}
	};
	client.OnReceivePackage = [&client](auto& recvData)
	{
		HandlePkg(&client, recvData);
	};
	client.ConnectEx("127.0.0.1", 12345);

	sw.Reset();

	return loop.Run();
}
