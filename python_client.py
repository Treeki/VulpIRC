# Yes, this source code is terrible.
# It's just something I've put together to test the server
# before I write a *real* client.

import sys, socket, ssl, threading, struct, time, zlib
from PyQt5 import QtCore, QtGui, QtWidgets

PRESET_COLOURS = [
	QtGui.QColor(255,255,255), # COL_IRC_WHITE = 0,
	QtGui.QColor(  0,  0,  0), # COL_IRC_BLACK = 1,
	QtGui.QColor(  0,  0,128), # COL_IRC_BLUE = 2,
	QtGui.QColor(  0,128,  0), # COL_IRC_GREEN = 3,
	QtGui.QColor(255,  0,  0), # COL_IRC_RED = 4,
	QtGui.QColor(128,  0,  0), # COL_IRC_BROWN = 5,
	QtGui.QColor(128,  0,128), # COL_IRC_PURPLE = 6,
	QtGui.QColor(128,128,  0), # COL_IRC_ORANGE = 7,
	QtGui.QColor(255,255,  0), # COL_IRC_YELLOW = 8,
	QtGui.QColor(  0,255,  0), # COL_IRC_LIME = 9,
	QtGui.QColor(  0,128,128), # COL_IRC_TEAL = 10,
	QtGui.QColor(  0,255,255), # COL_IRC_CYAN = 11,
	QtGui.QColor(  0,  0,255), # COL_IRC_LIGHT_BLUE = 12,
	QtGui.QColor(255,  0,255), # COL_IRC_PINK = 13,
	QtGui.QColor(128,128,128), # COL_IRC_GREY = 14,
	QtGui.QColor(192,192,192), # COL_IRC_LIGHT_GREY = 15,
	QtGui.QColor(  0,  0,  0), # COL_DEFAULT_FG = 16,
	QtGui.QColor(255,255,255), # COL_DEFAULT_BG = 17,
	QtGui.QColor(102,  0,204), # COL_ACTION = 18,
	QtGui.QColor(  0,153,  0), # COL_JOIN = 19,
	QtGui.QColor(102,  0,  0), # COL_PART = 20,
	QtGui.QColor(102,  0,  0), # COL_QUIT = 21,
	QtGui.QColor(102,  0,  0), # COL_KICK = 22,
	QtGui.QColor( 51,102,153), # COL_CHANNEL_NOTICE = 23,
]

protocolVer = 3
sock = None
authed = False
sessionKey = b'\0'*16
nextID = 1
lastReceivedPacketID = 0
packetCache = []
packetLock = threading.Lock()
password = ''

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
	global lastReceivedPacketID, authed, sessionKey, nextID, packetCache
	readbuf = b''

	sockCopy = sock

	app.postEvent(mainwin, PacketEvent(-1, '(Connected to server)'))
	while True:
		data = sockCopy.recv(1024)
		if not data:
			app.postEvent(mainwin, PacketEvent(-1, '(Disconnected from server)'))
			break

		readbuf += data

		pos = 0
		bufsize = len(readbuf)
		#print('[bufsize: %d]' % bufsize)
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
					app.postEvent(mainwin, PacketEvent(-2, None))
					app.postEvent(mainwin, PacketEvent(-1, 'Session started.'))
					nextID = 1
					packetCache = []
				elif type == 0x8002:
					app.postEvent(mainwin, PacketEvent(-1, 'Login error!'))
				elif type == 0x8003:
					app.postEvent(mainwin, PacketEvent(-1, 'Session resumed successfully.'))
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

		#print('[processed %d bytes]' % pos)
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
		self.output.setTextColor(PRESET_COLOURS[16])
		self.output.setTextBackgroundColor(PRESET_COLOURS[17])

		self.input = QtWidgets.QLineEdit(self)
		self.input.returnPressed.connect(self.handleLineEntered)

	def makeLayout(self):
		layout = QtWidgets.QVBoxLayout(self)
		layout.addWidget(self.output)
		layout.addWidget(self.input)

	enteredMessage = QtCore.pyqtSignal(str)
	def handleLineEntered(self):
		line = self.input.text()
		self.input.setText('')

		self.enteredMessage.emit(line)

	def pushMessage(self, msg, timestamp):
		ts = time.strftime('\x01[\x02\x10\x1D%H:%M:%S\x18\x01]\x02 ', time.localtime(timestamp))
		msg = ts + msg

		cursor = self.output.textCursor()

		isAtEnd = cursor.atEnd()
		cursor.movePosition(QtGui.QTextCursor.End)
		cursor.clearSelection()

		fmt = QtGui.QTextCharFormat()

		foreground = [None,None,None,None]
		background = [None,None,None,None]

		pos = 0
		build = ''
		l = len(msg)
		while pos < l:
			char = msg[pos]
			c = ord(char)
			if c == 7 or c == 10 or c >= 0x20:
				build += char
			else:
				cursor.insertText(build, fmt)
				build = ''

			if c == 1:
				fmt.setFontWeight(QtGui.QFont.Bold)
			elif c == 2:
				fmt.setFontWeight(QtGui.QFont.Normal)
			elif c == 3:
				fmt.setFontItalic(True)
			elif c == 4:
				fmt.setFontItalic(False)
			elif c == 5:
				fmt.setFontUnderline(True)
			elif c == 6:
				fmt.setFontUnderline(False)
			elif (c >= 0x10 and c <= 0x1F):
				layer = c & 3
				isBG = (True if ((c & 4) == 4) else False)
				isEnd = (True if ((c & 8) == 8) else False)

				col = None

				if not isEnd and (pos + 1) < l:
					bit1 = ord(msg[pos + 1])
					col = None
					if (bit1 & 1) == 1:
						col = PRESET_COLOURS[bit1 >> 1]
						pos += 1
					elif (pos + 3) < l:
						bit1 <<= 1
						bit2 = ord(msg[pos + 2]) << 1
						bit3 = ord(msg[pos + 3]) << 1
						pos += 3
						col = QtGui.QColor(bit1,bit2,bit3)

				if isBG:
					array = background
				else:
					array = foreground

				array[layer] = col
				maxcol = None
				for check in array:
					if check is not None:
						maxcol = check

				if isBG:
					if maxcol:
						fmt.setBackground(QtGui.QBrush(maxcol))
					else:
						fmt.clearBackground()
				else:
					if maxcol:
						fmt.setForeground(QtGui.QBrush(maxcol))
					else:
						fmt.clearForeground()


			pos += 1
		cursor.insertText(build, fmt)
		cursor.insertText('\n')

		if isAtEnd:
			self.output.setTextCursor(cursor)

class ChannelTab(WindowTab):
	class UserEntry:
		def __init__(self, nick, prefix, modes, listwidget):
			self.nick = nick
			self.prefix = prefix
			self.modes = modes
			self.item = QtWidgets.QListWidgetItem('', listwidget)
			self.syncItemText()

		def syncItemText(self):
			if self.prefix == 0:
				self.item.setText(self.nick)
			else:
				self.item.setText(chr(self.prefix) + self.nick)

	def __init__(self, parent=None):
		WindowTab.__init__(self, parent)

		self.topicLabel = QtWidgets.QLabel(self)
		self.topicLabel.setWordWrap(True)
		self.userList = QtWidgets.QListWidget(self)
		self.users = {}

	def makeLayout(self):
		sublayout = QtWidgets.QVBoxLayout()
		sublayout.addWidget(self.topicLabel)
		sublayout.addWidget(self.output)
		sublayout.addWidget(self.input)

		layout = QtWidgets.QHBoxLayout(self)
		layout.addLayout(sublayout, 1)
		layout.addWidget(self.userList)

	def readJunk(self, pdata, pos):
		userCount = u32.unpack_from(pdata, pos)[0]
		pos += 4

		users = {}
		for i in range(userCount):
			#prefix = pdata[pos]
			#pos += 1
			nicklen = u32.unpack_from(pdata, pos)[0]
			pos += 4
			nick = pdata[pos:pos+nicklen].decode('utf-8', 'replace')
			pos += nicklen
			modes = u32.unpack_from(pdata, pos)[0]
			pos += 4
			prefix = pdata[pos]
			pos += 1

			users[nick] = self.UserEntry(nick, prefix, modes, self.userList)

		self.users = users

		topiclen = u32.unpack_from(pdata, pos)[0]
		pos += 4
		self.topic = pdata[pos:pos+topiclen].decode('utf-8', 'replace')
		pos += topiclen

		self.topicLabel.setText(self.topic)

		return pos

	def addUsers(self, pdata):
		userCount = u32.unpack_from(pdata, 4)[0]
		pos = 8

		for i in range(userCount):
			nicklen = u32.unpack_from(pdata, pos)[0]
			pos += 4
			nick = pdata[pos:pos+nicklen].decode('utf-8', 'replace')
			pos += nicklen
			modes = u32.unpack_from(pdata, pos)[0]
			pos += 4
			prefix = pdata[pos]
			pos += 1

			self.users[nick] = self.UserEntry(nick, prefix, modes, self.userList)

	def removeUsers(self, pdata):
		userCount = u32.unpack_from(pdata, 4)[0]
		pos = 8
		if userCount == 0:
			self.users = {}
			self.userList.clear()
		else:
			for i in range(userCount):
				nicklen = u32.unpack_from(pdata, pos)[0]
				pos += 4
				nick = pdata[pos:pos+nicklen].decode('utf-8', 'replace')
				pos += nicklen
				print('Removing [%s]' % repr(nick))

				uo = self.users[nick]
				self.userList.takeItem(self.userList.row(uo.item))

				del self.users[nick]
	
	def renameUser(self, pdata):
		pos = 4
		nicklen = u32.unpack_from(pdata, pos)[0]
		pos += 4
		fromnick = pdata[pos:pos+nicklen].decode('utf-8', 'replace')
		pos += nicklen
		nicklen = u32.unpack_from(pdata, pos)[0]
		pos += 4
		tonick = pdata[pos:pos+nicklen].decode('utf-8', 'replace')

		uo = self.users[fromnick]
		del self.users[fromnick]
		self.users[tonick] = uo

		uo.nick = tonick
		uo.syncItemText()

	def changeUserMode(self, pdata):
		pos = 4
		nicklen = u32.unpack_from(pdata, pos)[0]
		pos += 4
		nick = pdata[pos:pos+nicklen].decode('utf-8', 'replace')
		pos += nicklen
		modes, prefix = struct.unpack_from('<IB', pdata, pos)

		uo = self.users[nick]
		uo.modes = modes
		uo.prefix = prefix
		uo.syncItemText()

	def changeTopic(self, pdata):
		tlen = u32.unpack_from(pdata, 4)[0]
		self.topic = pdata[8:8+tlen].decode('utf-8', 'replace')
		self.topicLabel.setText(self.topic)


class MainWindow(QtWidgets.QMainWindow):
	def __init__(self, parent=None):
		QtWidgets.QMainWindow.__init__(self, parent)

		self.setWindowTitle('VulpIRC Development Client')

		tb = self.addToolBar('Main')
		tb.addAction('Connect', self.handleConnect)
		tb.addAction('Disconnect', self.handleDisconnect)
		tb.addAction('Login', self.handleLogin)

		self.tabs = QtWidgets.QTabWidget(self)
		self.tabLookup = {}
		self.setCentralWidget(self.tabs)

		self.debugTab = WindowTab(self)
		self.debugTab.makeLayout()
		self.debugTab.enteredMessage.connect(self.handleDebug)
		self.tabs.addTab(self.debugTab, 'Debug')

	def event(self, event):
		if event.type() == QtCore.QEvent.User:
			event.accept()

			ptype = event.packetType
			pdata = event.packetData

			if ptype == -1:
				# Special type for messages coming from the reader
				# function. Messy as fuck, but doesn't matter in this
				# throwaway client
				self.debugTab.pushMessage(pdata, 0)
			elif ptype == -2:
				# Also a special type, this means that it's
				# a new session and we should delete all our
				# existing tabs and other state
				for wid,tab in self.tabLookup.items():
					self.tabs.removeTab(self.tabs.indexOf(tab))
				self.tabLookup = {}

			elif ptype == 1:
				strlen = u32.unpack_from(pdata, 0)[0]
				msg = pdata[4:4+strlen].decode('utf-8', 'replace')
				self.debugTab.pushMessage(msg, 0)
			elif ptype == 0x100 or ptype == 0x104 or ptype == 0x80 or ptype == 0x81:
				if ptype == 0x104 or ptype == 0x81:
					pdata = zlib.decompress(pdata[8:])
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
						timestamp = u32.unpack_from(pdata, pos)[0]
						pos += 4
						msglen = u32.unpack_from(pdata, pos)[0]
						pos += 4
						msg = pdata[pos:pos+msglen].decode('utf-8', 'replace')
						pos += msglen
						msgs.append((timestamp, msg))

					if wtype == 1 or wtype == 3:
						tab = WindowTab(self)
					elif wtype == 2:
						tab = ChannelTab(self)
						pos = tab.readJunk(pdata, pos)

					tab.makeLayout()
					tab.winID = wid
					tab.enteredMessage.connect(self.handleWindowInput)
					self.tabs.addTab(tab, wtitle)
					self.tabLookup[wid] = tab

					for timestamp, msg in msgs:
						tab.pushMessage(msg, timestamp)
			elif ptype == 0x101:
				# WINDOW CLOSE
				wndCount = u32.unpack_from(pdata, 0)[0]
				pos = 4

				for i in range(wndCount):
					wid = u32.unpack_from(pdata, pos)[0]
					pos += 4

					if wid in self.tabLookup:
						tab = self.tabLookup[wid]
						self.tabs.removeTab(self.tabs.indexOf(tab))
						del self.tabLookup[wid]

			elif ptype == 0x102:
				# WINDOW MESSAGES
				wndID, timestamp, priority, ack, msglen = struct.unpack_from('<IIbbI', pdata, 0)
				msg = pdata[14:14+msglen].decode('utf-8', 'replace')
				self.tabLookup[wndID].pushMessage(msg, timestamp)
			elif ptype == 0x103:
				# WINDOW RENAME
				wndID, msglen = struct.unpack_from('<II', pdata, 0)
				title = pdata[8:8+msglen].decode('utf-8', 'replace')

				tabObj = self.tabLookup[wndID]
				self.tabs.setTabText(self.tabs.indexOf(tabObj), title)

			elif ptype == 0x120:
				# Add users to channel
				wndID = u32.unpack_from(pdata, 0)[0]
				self.tabLookup[wndID].addUsers(pdata)
			elif ptype == 0x121:
				# Remove users from channel
				wndID = u32.unpack_from(pdata, 0)[0]
				self.tabLookup[wndID].removeUsers(pdata)
			elif ptype == 0x122:
				# Rename user in channel
				wndID = u32.unpack_from(pdata, 0)[0]
				self.tabLookup[wndID].renameUser(pdata)
			elif ptype == 0x123:
				# Change user modes in channel
				wndID = u32.unpack_from(pdata, 0)[0]
				self.tabLookup[wndID].changeUserMode(pdata)
			elif ptype == 0x124:
				# Change topic in channel
				wndID = u32.unpack_from(pdata, 0)[0]
				self.tabLookup[wndID].changeTopic(pdata)

			return True
		else:
			return QtWidgets.QMainWindow.event(self, event)

	def handleConnect(self):
		global sock
		try:
			basesock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
			basesock.connect((hostname, 5454))
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
		encPW = password.encode('utf-8')
		piece1 = struct.pack('<III', protocolVer, lastReceivedPacketID, len(encPW))
		piece2 = struct.pack('16s', sessionKey)
		writePacket(0x8001, piece1 + encPW + piece2, True)

	def handleDebug(self, text):
		with packetLock:
			data = str(text).encode('utf-8')
			writePacket(1, struct.pack('<I', len(data)) + data)

	def handleWindowInput(self, text):
		wid = self.sender().winID
		with packetLock:
			if text == '/close':
				writePacket(0x101, u32.pack(wid))
			else:
				data = str(text).encode('utf-8')
				writePacket(0x102, struct.pack('<IbI', wid, 0, len(data)) + data)



hostname = sys.argv[1]
if len(sys.argv) > 2:
	password = sys.argv[2]
else:
	print('No password entered on command line!')

app = QtWidgets.QApplication(sys.argv)

mainwin = MainWindow()
mainwin.show()

app.exec_()

