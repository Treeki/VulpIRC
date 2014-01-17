import socket, ssl, threading, struct

protocolVer = 1
sock = None
authed = False
sessionKey = b'\0'*16
nextID = 1
lastReceivedPacketID = 0
packetCache = []
packetLock = threading.Lock()

class Packet:
	def __init__(self, type, data):
		global nextID

		self.type = type
		self.data = data
		if (type & 0x8000) == 0:
			self.id = nextID
			nextID = nextID + 1

	def sendOverWire(self):
		header = struct.pack('<HHI', self.type, 0, len(self.data))
		if (self.type & 0x8000) == 0:
			extHeader = struct.pack('<II', self.id, lastReceivedPacketID)
		else:
			extHeader = b''

		sock.sendall(header)
		if extHeader:
			sock.sendall(extHeader)
		sock.sendall(self.data)


def clearCachedPackets(pid):
	for packet in packetCache[:]:
		if packet.id <= pid:
			packetCache.remove(packet)

def reader():
	global lastReceivedPacketID, authed, sessionKey
	readbuf = b''

	print('(Connected)')
	while True:
		data = sock.recv(1024)
		if not data:
			print('(Disconnected)')
			break

		readbuf += data

		pos = 0
		bufsize = len(readbuf)
		print('[bufsize: %d]' % bufsize)
		while True:
			if (pos + 8) > bufsize:
				break

			type, reserved, size = struct.unpack_from('<HHI', readbuf, pos)

			extHeaderSize = 8 if ((type & 0x8000) == 0) else 0
			if (pos + 8 + extHeaderSize + size) > bufsize:
				break

			pos += 8
			with packetLock:
				if ((type & 0x8000) == 0):
					pid, lastReceivedByServer = struct.unpack_from('<II', readbuf, pos)
					pos += 8

					lastReceivedPacketID = pid
					clearCachedPackets(lastReceivedByServer)

				packetdata = data[pos:pos+size]
				print('0x%x : %d bytes : %s' % (type, size, packetdata))

				if type == 0x8001:
					sessionKey = packetdata
					authed = True
				elif type == 0x8003:
					authed = True
					pid = struct.unpack('<I', packetdata)
					clearCachedPackets(pid)
					try:
						for packet in packetCache:
							packet.sendOverWire()
					except:
						pass
				elif type == 1:
					print(packetdata.decode('utf-8'))

			pos += size

		print('[processed %d bytes]' % pos)
		readbuf = readbuf[pos:]

def writePacket(type, data, allowUnauthed=False):
	packet = Packet(type, data)
	if (type & 0x8000) == 0:
		packetCache.append(packet)
	try:
		if authed or allowUnauthed:
			packet.sendOverWire()
	except:
		pass



while True:
	bit = input()
	bits = bit.split(' ', 1)
	cmd = bits[0]

	with packetLock:
		print('{')
		if cmd == 'connect':
			try:
				basesock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
				basesock.connect(('localhost', 5454))
				#sock = ssl.wrap_socket(basesock)
				sock = basesock
				thd = threading.Thread(None, reader)
				thd.start()
			except Exception as e:
				print(e)
		elif cmd == 'disconnect':
			sock.shutdown(socket.SHUT_RDWR)
			sock.close()
			sock = None
			authed = False
		elif cmd == 'login':
			writePacket(0x8001, struct.pack('<II 16s', protocolVer, lastReceivedPacketID, sessionKey), True)
		elif cmd == 'cmd':
			data = bits[1].encode('utf-8')
			writePacket(1, struct.pack('<I', len(data)) + data)
		elif cmd == 'quit':
			break
		print('}')

