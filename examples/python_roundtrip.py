#!/usr/bin/env python3
"""Direct construction and borrowed reading; every buffer ends before transfer."""
import struct
from elite_ringbuffer import EliteShm

for mode in ('spsc','ncq'):
    with EliteShm(mode, capacity=8, max_payload=64) as shm:
        with shm.producer() as producer, shm.consumer() as consumer:
            with producer.reserve() as write:
                with write.buffer as view:
                    struct.pack_into('<QQ', view, 0, 42, 99)
                write.commit(16, message_type=1, message_id=42)
            with consumer.borrow() as read:
                with read.buffer as view:
                    values=struct.unpack_from('<QQ',view)
                if values!=(42,99) or read.message_id!=42:
                    raise RuntimeError('round-trip mismatch')
                print(mode,values,read.message_id)
