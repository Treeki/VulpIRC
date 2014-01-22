# Yes, this source code is terrible.
# It's just something I've put together to test the server
# before I write a *real* client.

import sys, socket, ssl, threading, struct
from PyQt5 import QtCore, QtGui, QtWidgets

protocolVer = 1
sock = None
authed = False
sessionKey = b'\0'*16
nextID = 1
lastReceivedPacketID = 0
packetCache = []
packetLock = threading.Lock()

u32 = struct.Struct('<I')

class Packet:
	def __init__(self, type, data):
		global nextID

		self.type = type
		self.data = data
		if (type & 0x8000) == 0:
			self.id = nextID
			nextID = nextID + 1

	def sendOverWire(self, sock):
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

	sockCopy = sock

	print('(Connected)')
	while True:
		data = sockCopy.recv(1024)
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

				packetdata = readbuf[pos:pos+size]
				print('0x%x : %d bytes : %s' % (type, size, packetdata))

				if type == 0x8001:
					sessionKey = packetdata
					authed = True
				elif type == 0x8002:
					print('FAILED!')
				elif type == 0x8003:
					authed = True
					pid = u32.unpack(packetdata)[0]
					clearCachedPackets(pid)
					try:
						for packet in packetCache:
							packet.sendOverWire(sockCopy)
					except:
						pass
				else:
					# Horrible kludge. I'm sorry.
					# I didn't feel like rewriting this to use
					# QObject and QThread. :(
					packetEvent = PacketEvent(type, packetdata)
					app.postEvent(mainwin, packetEvent)

			pos += size

		print('[processed %d bytes]' % pos)
		readbuf = readbuf[pos:]

def writePacket(type, data, allowUnauthed=False):
	packet = Packet(type, data)
	if (type & 0x8000) == 0:
		packetCache.append(packet)
	try:
		if authed or allowUnauthed:
			packet.sendOverWire(sock)
	except:
		pass


class PacketEvent(QtCore.QEvent):
	def __init__(self, ptype, pdata):
		QtCore.QEvent.__init__(self, QtCore.QEvent.User)
		self.packetType = ptype
		self.packetData = pdata


class WindowTab(QtWidgets.QWidget):
	def __init__(self, parent=None):
		QtWidgets.QWidget.__init__(self, parent)

		self.output = QtWidgets.QTextEdit(self)
		self.output.setReadOnly(True)
		self.input = QtWidgets.QLineEdit(self)
		self.input.returnPressed.connect(self.handleLineEntered)

		layout = QtWidgets.QVBoxLayout(self)
		layout.addWidget(self.output)
		layout.addWidget(self.input)

	enteredMessage = QtCore.pyqtSignal(str)
	def handleLineEntered(self):
		line = self.input.text()
		self.input.setText('')

		self.enteredMessage.emit(line)

	def pushMessage(self, msg):
		cursor = self.output.textCursor()

		isAtEnd = cursor.atEnd()
		cursor.movePosition(QtGui.QTextCursor.End)
		cursor.clearSelection()
		cursor.insertText(msg)
		cursor.insertText('\n')

		if isAtEnd:
			self.output.setTextCursor(cursor)


class MainWindow(QtWidgets.QMainWindow):
	def __init__(self, parent=None):
		QtWidgets.QMainWindow.__init__(self, parent)

		self.setWindowTitle('Ninjifox\'s IRC Client Test')

		tb = self.addToolBar('Main')
		tb.addAction('Connect', self.handleConnect)
		tb.addAction('Disconnect', self.handleDisconnect)
		tb.addAction('Login', self.handleLogin)

		self.tabs = QtWidgets.QTabWidget(self)
		self.tabLookup = {}
		self.setCentralWidget(self.tabs)

		self.debugTab = WindowTab(self)
		self.debugTab.enteredMessage.connect(self.handleDebug)
		self.tabs.addTab(self.debugTab, 'Debug')

	def event(self, event):
		if event.type() == QtCore.QEvent.User:
			event.accept()

			ptype = event.packetType
			pdata = event.packetData

			if ptype == 1:
				strlen = u32.unpack_from(pdata, 0)[0]
				msg = pdata[4:4+strlen].decode('utf-8', 'replace')
				self.debugTab.pushMessage(msg)
			elif ptype == 0x100:
				# ADD WINDOWS
				wndCount = u32.unpack_from(pdata, 0)[0]
				pos = 4

				for i in range(wndCount):
					wtype, wid, wtlen = struct.unpack_from('<III', pdata, pos)
					pos += 12
					wtitle = pdata[pos:pos+wtlen].decode('utf-8', 'replace')
					pos += wtlen
					msgCount = u32.unpack_from(pdata, pos)[0]
					pos += 4
					msgs = []
					for j in range(msgCount):
						msglen = u32.unpack_from(pdata, pos)[0]
						pos += 4
						msg = pdata[pos:pos+msglen].decode('utf-8', 'replace')
						pos += msglen
						msgs.append(msg)

					tab = WindowTab(self)
					tab.winID = wid
					tab.enteredMessage.connect(self.handleWindowInput)
					self.tabs.addTab(tab, wtitle)
					self.tabLookup[wid] = tab
					tab.pushMessage('\n'.join(msgs))
			elif ptype == 0x102:
				# WINDOW MESSAGES
				wndID, msglen = struct.unpack_from('<II', pdata, 0)
				msg = pdata[8:8+msglen].decode('utf-8', 'replace')
				self.tabLookup[wndID].pushMessage(msg)

			return True
		else:
			return QtWidgets.QMainWindow.event(self, event)

	def handleConnect(self):
		global sock
		try:
			basesock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
			basesock.connect(('localhost', 5454))
			#sock = ssl.wrap_socket(basesock)
			sock = basesock
			thd = threading.Thread(None, reader)
			thd.daemon = True
			thd.start()
		except Exception as e:
			print(e)

	def handleDisconnect(self):
		global sock, authed
		sock.shutdown(socket.SHUT_RDWR)
		sock.close()
		sock = None
		authed = False

	def handleLogin(self):
		writePacket(0x8001, struct.pack('<II 16s', protocolVer, lastReceivedPacketID, sessionKey), True)

	def handleDebug(self, text):
		with packetLock:
			data = str(text).encode('utf-8')
			writePacket(1, struct.pack('<I', len(data)) + data)

	def handleWindowInput(self, text):
		wid = self.sender().winID
		with packetLock:
			data = str(text).encode('utf-8')
			writePacket(0x102, struct.pack('<II', wid, len(data)) + data)


app = QtWidgets.QApplication(sys.argv)

mainwin = MainWindow()
mainwin.show()

app.exec_()

