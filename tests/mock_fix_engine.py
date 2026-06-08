import asyncio
import logging
import time

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("Mock-FIX-Engine")

# Standard FIX delimiter
SOH = b'\x01'

def build_new_order_single(seq_num: int) -> bytes:
    """Builds a dummy FIX New Order Single message."""
    # 8=BeginString, 9=BodyLength, 35=MsgType (D=New Order Single), 34=MsgSeqNum, 10=Checksum
    body = f"35=D{SOH.decode()}34={seq_num}{SOH.decode()}49=CLIENT{SOH.decode()}56=BIST{SOH.decode()}11=ORDER_{seq_num}{SOH.decode()}"
    msg = f"8=FIX.4.4{SOH.decode()}9={len(body)}{SOH.decode()}{body}"
    
    # Calculate simple checksum (sum of bytes % 256)
    checksum = sum(msg.encode()) % 256
    msg += f"10={checksum:03d}{SOH.decode()}"
    return msg.encode()

async def mock_bist_server(host, port):
    """Simulates the BIST FIX server."""
    messages_received = 0
    start_time = None
    
    async def handle_connection(reader, writer):
        nonlocal messages_received, start_time
        logger.info("BIST Server: Client connected.")
        while True:
            data = await reader.read(65536)
            if not data:
                break
            
            if start_time is None:
                start_time = time.time()
                
            # Count FIX messages
            messages_received += data.count(b'10=')
            
            elapsed = time.time() - start_time
            if elapsed >= 1.0:
                logger.info(f"BIST Server: Received {messages_received} msgs/sec")
                messages_received = 0
                start_time = time.time()
                
    server = await asyncio.start_server(handle_connection, host, port)
    logger.info(f"Mock BIST FIX Server listening on {host}:{port}")
    async with server:
        await server.serve_forever()

async def mock_fix_client(host, port, rate_limit=10000):
    """Simulates a High Frequency Trading client sending FIX messages."""
    await asyncio.sleep(1) # wait for server to start
    logger.info(f"Mock FIX Client connecting to Proxy at {host}:{port}")
    reader, writer = await asyncio.open_connection(host, port)
    
    seq_num = 1
    logger.info(f"Mock FIX Client: Starting to blast {rate_limit} msgs/sec")
    
    test_start = time.time()
    while True:
        if time.time() - test_start > 5.0:
            logger.info("Mock FIX Client: 5 saniyelik test tamamlandı, durduruluyor.")
            break
            
        start_time = time.time()
        for _ in range(rate_limit):
            msg = build_new_order_single(seq_num)
            writer.write(msg)
            seq_num += 1
        
        await writer.drain()
        
        elapsed = time.time() - start_time
        if elapsed < 1.0:
            await asyncio.sleep(1.0 - elapsed)

async def main():
    # BIST Server listens on 5003
    server_task = asyncio.create_task(mock_bist_server('127.0.0.1', 5003))
    
    # Client connects to Client Proxy on 5001
    client_task = asyncio.create_task(mock_fix_client('127.0.0.1', 5001, rate_limit=10000))
    
    await asyncio.gather(server_task, client_task)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Mock Engine stopped.")
