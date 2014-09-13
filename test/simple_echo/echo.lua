
server.setCallback(function (context)
	if context.event == "socket_read" then
		context.oBuf:append(context.iBuf:extract())
	end
end)

server.addServerSocket("0.0.0.0", 12345)
