import asyncio
import websockets

async def client():
    async with websockets.connect('ws://localhost:8000') as channel:
        for i in range(64):
            # 发送消息
            message = f"message {i} transmitted via websocket"
            await channel.send(message)

            # 接收消息
            received = await channel.recv()
            print(f"Received: {received}")

asyncio.run(client())
