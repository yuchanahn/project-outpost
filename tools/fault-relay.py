"""Bounded loopback UDP relay; impair both directions without touching ycnet."""
import argparse
import heapq
import json
import random
import selectors
import socket
import time

p = argparse.ArgumentParser()
p.add_argument('--seconds', type=float, default=16)
p.add_argument('--seed', type=int, default=42)
p.add_argument('--loss', type=float, default=0.10)
p.add_argument('--duplicate', type=float, default=0.05)
p.add_argument('--jitter-ms', type=float, default=80)
a = p.parse_args()
if not (0 <= a.loss < 1 and 0 <= a.duplicate <= 1 and a.jitter_ms >= 0 and a.seconds > 0):
    p.error('invalid impairment settings')
rng = random.Random(a.seed)
selector = selectors.DefaultSelector()
listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
listener.bind(('127.0.0.1', 27016))
listener.setblocking(False)
selector.register(listener, selectors.EVENT_READ, None)
peers, pending = {}, []
stats = dict(received=0, dropped=0, duplicated=0, forwarded=0, reordered=0, max_queue=0)
serial = 0
last = {}
end = time.monotonic() + a.seconds
print('RELAY_READY port=27016', flush=True)
try:
    while time.monotonic() < end:
        for key, _ in selector.select(0.001):
            try:
                data, source = key.fileobj.recvfrom(65535)
            except (ConnectionResetError, BlockingIOError):
                # Windows reports UDP ICMP port-unreachable after a peer exits.
                continue
            if key.fileobj is listener:
                if source not in peers:
                    if len(peers) >= 32:
                        continue
                    upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    upstream.bind(('127.0.0.1', 0))
                    upstream.setblocking(False)
                    peers[source] = upstream
                    selector.register(upstream, selectors.EVENT_READ, source)
                outgoing, target = peers[source], ('127.0.0.1', 27015)
            else:
                if source != ('127.0.0.1', 27015):
                    continue
                outgoing, target = listener, key.data
            stats['received'] += 1
            if rng.random() < a.loss:
                stats['dropped'] += 1
                continue
            copies = 2 if rng.random() < a.duplicate else 1
            stats['duplicated'] += copies - 1
            for _ in range(copies):
                serial += 1
                heapq.heappush(pending, (time.monotonic() + rng.random()*a.jitter_ms/1000,
                                         serial, outgoing, target, data))
            stats['max_queue'] = max(stats['max_queue'], len(pending))
        while pending and pending[0][0] <= time.monotonic():
            _, order, outgoing, target, data = heapq.heappop(pending)
            flow = (outgoing.fileno(), target)
            if order < last.get(flow, 0):
                stats['reordered'] += 1
            last[flow] = max(order, last.get(flow, 0))
            outgoing.sendto(data, target)
            stats['forwarded'] += 1
finally:
    for upstream in peers.values():
        upstream.close()
    listener.close()
    selector.close()
    print(json.dumps(dict(settings=vars(a), flows=len(peers), **stats)), flush=True)
