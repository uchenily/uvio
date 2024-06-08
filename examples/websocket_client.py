import asyncio
import websockets

async def client():
    async with websockets.connect('ws://localhost:8000') as channel:
        for i in range(64):
            # 发送消息
            # message = f"message {i} transmitted via websocket"
            # message = "a"*100000 # (payload len 64bits)
            # message = "b"*1024 # (payload len 16bits)
            message = "c"*64 # (payload len 7bits)
            await channel.send(message)

            # 接收消息
            received = await channel.recv()
            print(f"Received: {received}")

asyncio.run(client())
