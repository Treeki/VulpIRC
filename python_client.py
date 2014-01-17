import socket, ssl, threading

basesock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
basesock.connect(('localhost', 5454))
sock = ssl.wrap_socket(basesock)

def reader():
	print('(Connected)')
	while True:
		data = sock.read()
		if data:
			print(data)
		else:
			print('(Disconnected)')
			break

thd = threading.Thread(None, reader)
thd.start()

while True:
	bit = input()
	sock.write((bit + '\n').encode('utf-8'))

