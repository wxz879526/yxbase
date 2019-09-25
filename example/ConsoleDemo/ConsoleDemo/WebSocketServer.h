#pragma once

#include <iostream>
#include "net/server/http_server.h"
#include "net/socket/tcp_server_socket.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"

constexpr int kBufferSize = 100 * 1024 * 1024;

class WebSocketServer : public net::HttpServer::Delegate
{
public:
	enum WebSocketRequestAction {
		kAccept,
		kNotFound,
		kClose
	};

	enum WebSocketMessageAction {
		kEchoMessage,
		kCloseOnMessage
	};

	WebSocketServer()
		: thread_("WebSocketServerThread")
		, all_closed_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
			base::WaitableEvent::InitialState::SIGNALED)
		, request_action_(kAccept)
		, message_action_(kEchoMessage)
	{

	}

	~WebSocketServer() override
	{

	}

	bool Start()
	{
		base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
		bool thread_started = thread_.StartWithOptions(options);
		if (!thread_started)
			return false;

		bool success;
		base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
			base::WaitableEvent::InitialState::NOT_SIGNALED);
		thread_.task_runner()->PostTask(FROM_HERE,
			base::Bind(&WebSocketServer::StartOnServerThread, base::Unretained(this),
				&success, &event));
		event.Wait();

		return success;
	}

	void Stop()
	{
		if (!thread_.IsRunning())
			return;

		base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
			base::WaitableEvent::InitialState::NOT_SIGNALED);
		thread_.task_runner()->PostTask(FROM_HERE,
			base::Bind(&WebSocketServer::StopOnServerThread, base::Unretained(this),
				&event));
		event.Wait();
		thread_.Stop();
	}

	bool WaitForConnectionToClose()
	{
		return all_closed_event_.TimedWait(base::TimeDelta::FromSeconds(10));
	}

	void SetRequestAction(WebSocketRequestAction action)
	{
		base::AutoLock lock(action_lock_);
		request_action_ = action;
	}

	void SetMessageAction(WebSocketMessageAction action)
	{
		base::AutoLock lock(action_lock_);
		message_action_ = action;
	}

	void SetMessageCallback(base::Closure &cb)
	{
		base::AutoLock lock(action_lock_);
		message_cb_ = cb;
	}

	GURL web_socket_url() const
	{
		base::AutoLock lock(url_lock_);
		return web_socket_url_;
	}

	virtual void OnConnect(int connection_id) override
	{
		server_->SetSendBufferSize(connection_id, kBufferSize);
		server_->SetReceiveBufferSize(connection_id, kBufferSize);
		
		std::cout << "WebSocketServer OnConnect: " << connection_id << std::endl;
	}

	virtual void OnHttpRequest(int connection_id, const net::HttpServerRequestInfo& info) override
	{
		
	}

	virtual void OnWebSocketRequest(int connection_id, const net::HttpServerRequestInfo& info) override
	{
		WebSocketRequestAction action;
		{
			base::AutoLock lock(action_lock_);
			action = request_action_;
		}

		connections_.insert(connection_id);
		all_closed_event_.Reset();

		std::cout << "WebSocketServer OnWebSocketRequest: " << connection_id << std::endl;

		switch (action)
		{
		case kAccept:
			server_->AcceptWebSocket(connection_id, info);
			break;
		case kNotFound:
			server_->Send404(connection_id);
			break;
		case kClose:
			server_->Close(connection_id);
			break;
		}
	}

	virtual void OnWebSocketMessage(int connection_id, const std::string& data) override
	{
		WebSocketMessageAction action;
		base::Closure callback;
		{
			base::AutoLock lock(action_lock_);
			action = message_action_;
			callback = base::ResetAndReturn(&message_cb_);
		}

		std::cout << "WebSocketServer OnWebSocketMessage:" << connection_id << " data:" << data << std::endl;

		if (!callback.is_null())
			callback.Run();

		switch (action)
		{
		case WebSocketServer::kEchoMessage:
			server_->SendOverWebSocket(connection_id, "Message From Server");
			break;
		case WebSocketServer::kCloseOnMessage:
			server_->Close(connection_id);
			break;
		default:
			break;
		}
	}

	virtual void OnClose(int connection_id) override
	{
		connections_.erase(connection_id);
		if (connections_.empty())
			all_closed_event_.Signal();
	}

private:
	void StartOnServerThread(bool *success, base::WaitableEvent *event)
	{
		std::unique_ptr<net::ServerSocket> server_socket(
		new net::TCPServerSocket(nullptr, net::NetLogSource()));
		server_socket->ListenWithAddressAndPort("127.0.0.1", 8888, 1);;
		server_.reset(new net::HttpServer(std::move(server_socket), this));

		net::IPEndPoint address;
		int error = server_->GetLocalAddress(&address);

		if (net::OK == error)
		{
			std::cout << "WebSocketServer Started on port:" << address.port() << std::endl;
			base::AutoLock lock(url_lock_);
			web_socket_url_ = GURL(base::StringPrintf("ws://127.0.0.1:%d", address.port()));
		}
		else
		{
			server_.reset(nullptr);
		}

		*success = (server_.get() != nullptr);
		event->Signal();
	}

	void StopOnServerThread(base::WaitableEvent *event)
	{
		server_.reset(nullptr);
		event->Signal();
	}

	base::Thread thread_;

	std::unique_ptr<net::HttpServer> server_;

	std::set<int> connections_;

	base::WaitableEvent all_closed_event_;

	mutable base::Lock url_lock_;
	GURL web_socket_url_;

	base::Lock action_lock_;
	WebSocketRequestAction request_action_;
	WebSocketMessageAction message_action_;
	base::Closure message_cb_;

	DISALLOW_COPY_AND_ASSIGN(WebSocketServer);
};