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
	char m_preAllocBuffer[0x200];

public:
	Buffer() {
		m_data = m_preAllocBuffer;
		m_freeBuffer = false;
		m_size = 0;
		m_capacity = sizeof(m_preAllocBuffer);
	}

	~Buffer() {
		if ((m_data != NULL) && m_freeBuffer) {
			delete[] m_data;
			m_data = NULL;
		}
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
	void writeU16(uint32_t v) { append((const char *)&v, 2); }
	void writeU8(uint32_t v) { append((const char *)&v, 1); }
	void writeS32(uint32_t v) { append((const char *)&v, 4); }
	void writeS16(uint32_t v) { append((const char *)&v, 2); }
	void writeS8(uint32_t v) { append((const char *)&v, 1); }

	void writeStr(const char *data, int size = -1) {
		if (size == -1)
			size = strlen(data);
		writeU32(size);
		append(data, size);
	}
};

#endif /* BUFFER_H */
