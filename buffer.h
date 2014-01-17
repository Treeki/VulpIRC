#ifndef BUFFER_H
#define BUFFER_H 

#include <string.h>
#include <stdint.h>

class Buffer {
private:
	char *m_data;
	bool m_freeBuffer;
	int m_size;
	int m_capacity;
	int m_readPointer;
	char m_preAllocBuffer[0x200];

public:
	Buffer() {
		m_data = m_preAllocBuffer;
		m_freeBuffer = false;
		m_size = 0;
		m_capacity = sizeof(m_preAllocBuffer);
		m_readPointer = 0;
	}

	~Buffer() {
		if ((m_data != NULL) && m_freeBuffer) {
			delete[] m_data;
			m_data = NULL;
		}
	}

	void useExistingBuffer(char *data, int size) {
		if (m_freeBuffer)
			delete[] m_data;

		m_data = data;
		m_freeBuffer = false;
		m_size = size;
		m_capacity = size;
		m_readPointer = 0;
	}

	char *data() const { return m_data; }
	int size() const { return m_size; }
	int capacity() const { return m_capacity; }

	void setCapacity(int capacity) {
		if (capacity == m_capacity)
			return;

		// Trim the size down if it's too big to fit
		if (m_size > capacity)
			m_size = capacity;

		char *newBuf = new char[capacity];

		if (m_data != NULL) {
			memcpy(newBuf, m_data, m_size);
			if (m_freeBuffer)
				delete[] m_data;
		}

		m_data = newBuf;
		m_capacity = capacity;
		m_freeBuffer = true;
	}

	void clear() {
		m_size = 0;
	}
	void append(const char *data, int size) {
		if (size <= 0)
			return;

		int requiredSize = m_size + size;
		if (requiredSize > m_capacity)
			setCapacity(requiredSize + 0x100);

		memcpy(&m_data[m_size], data, size);
		m_size += size;
	}
	void append(const Buffer &buf) {
		append(buf.data(), buf.size());
	}
	void resize(int size) {
		if (size > m_capacity)
			setCapacity(size + 0x100);
		m_size = size;
	}

	void trimFromStart(int amount) {
		if (amount <= 0)
			return;
		if (amount >= m_size) {
			clear();
			return;
		}

		memmove(m_data, &m_data[amount], m_size - amount);
		m_size -= amount;
	}


	void writeU32(uint32_t v) { append((const char *)&v, 4); }
	void writeU16(uint16_t v) { append((const char *)&v, 2); }
	void writeU8(uint8_t v) { append((const char *)&v, 1); }
	void writeS32(int32_t v) { append((const char *)&v, 4); }
	void writeS16(int16_t v) { append((const char *)&v, 2); }
	void writeS8(int8_t v) { append((const char *)&v, 1); }

	void writeStr(const char *data, int size = -1) {
		if (size == -1)
			size = strlen(data);
		writeU32(size);
		append(data, size);
	}

	void readSeek(int pos) {
		m_readPointer = pos;
	}
	int readTell() const {
		return m_readPointer;
	}
	bool readRemains(int size) const {
		if ((size > 0) && ((m_readPointer + size) <= m_size))
			return true;
		return false;
	}
	void read(char *output, int size) {
		if ((m_readPointer + size) > m_size) {
			// Not enough space to read the whole thing...!
			int copy = m_size - m_readPointer;
			if (copy > 0)
				memcpy(output, &m_data[m_readPointer], copy);
			memset(&output[copy], 0, size - copy);
			m_readPointer = m_size;
		} else {
			memcpy(output, &m_data[m_readPointer], size);
			m_readPointer += size;
		}
	}
	uint32_t readU32() { uint32_t v; read((char *)&v, 4); return v; }
	uint16_t readU16() { uint16_t v; read((char *)&v, 2); return v; }
	uint8_t readU8() { uint8_t v; read((char *)&v, 1); return v; }
	int32_t readS32() { int32_t v; read((char *)&v, 4); return v; }
	int16_t readS16() { int16_t v; read((char *)&v, 2); return v; }
	int8_t readS8() { int8_t v; read((char *)&v, 1); return v; }
};

#endif /* BUFFER_H */
