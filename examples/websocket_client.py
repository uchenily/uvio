import asyncio
import websockets

async def client():
    async with websockets.connect('ws://localhost:8000') as channel:
        # 发送消息
        print('Sending: Hello, Server!')
        await channel.send('Hello, Server!')

        # 接收消息
        response = await channel.recv()
        print(f'Received: {response}')

        # 发送消息
        print('Sending: another message!')
        await channel.send('another message!')

        # 接收消息
        response = await channel.recv()
        print(f'Received: {response}')

asyncio.run(client())
