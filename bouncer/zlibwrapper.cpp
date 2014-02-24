#include "zlibwrapper.h"
#include <zlib.h>

bool ZlibWrapper::compress(const Buffer &input, Buffer &output) {
	// Reserve space for the compressed size
	output.writeU32(input.size());
	output.writeU32(0);
	int outputDataStartPos = output.size();

	z_stream stream;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK)
		return false;

	stream.next_in = (uint8_t *)input.data();
	stream.avail_in = input.size();

	const int chunkSize = 32768;
	char chunk[chunkSize];

	do {
		stream.next_out = (uint8_t *)chunk;
		stream.avail_out = chunkSize;
		deflate(&stream, Z_FINISH);

		output.append(chunk, chunkSize - stream.avail_out);
	} while (stream.avail_out == 0);

	uint32_t bits = stream.total_out;
	memcpy(&output.data()[outputDataStartPos - 4], &bits, 4);

	deflateEnd(&stream);

	return true;
}
