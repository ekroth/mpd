/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "DecoderBuffer.hxx"
#include "DecoderAPI.hxx"
#include "util/ConstBuffer.hxx"
#include "util/VarSize.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct DecoderBuffer {
	Decoder *decoder;
	InputStream *is;

	/** the allocated size of the buffer */
	size_t size;

	/** the current length of the buffer */
	size_t length;

	/** number of bytes already consumed at the beginning of the
	    buffer */
	size_t consumed;

	/** the actual buffer (dynamic size) */
	unsigned char data[sizeof(size_t)];

	DecoderBuffer(Decoder *_decoder, InputStream &_is,
		      size_t _size)
		:decoder(_decoder), is(&_is),
		 size(_size), length(0), consumed(0) {}
};

DecoderBuffer *
decoder_buffer_new(Decoder *decoder, InputStream &is,
		   size_t size)
{
	assert(size > 0);

	return NewVarSize<DecoderBuffer>(sizeof(DecoderBuffer::data),
					 size,
					 decoder, is, size);
}

void
decoder_buffer_free(DecoderBuffer *buffer)
{
	assert(buffer != nullptr);

	DeleteVarSize(buffer);
}

const InputStream &
decoder_buffer_get_stream(const DecoderBuffer *buffer)
{
	return *buffer->is;
}

void
decoder_buffer_clear(DecoderBuffer *buffer)
{
	buffer->length = buffer->consumed = 0;
}

static void
decoder_buffer_shift(DecoderBuffer *buffer)
{
	assert(buffer->consumed > 0);

	buffer->length -= buffer->consumed;
	memmove(buffer->data, buffer->data + buffer->consumed, buffer->length);
	buffer->consumed = 0;
}

bool
decoder_buffer_fill(DecoderBuffer *buffer)
{
	size_t nbytes;

	if (buffer->consumed > 0)
		decoder_buffer_shift(buffer);

	if (buffer->length >= buffer->size)
		/* buffer is full */
		return false;

	nbytes = decoder_read(buffer->decoder, *buffer->is,
			      buffer->data + buffer->length,
			      buffer->size - buffer->length);
	if (nbytes == 0)
		/* end of file, I/O error or decoder command
		   received */
		return false;

	buffer->length += nbytes;
	assert(buffer->length <= buffer->size);

	return true;
}

static const void *
decoder_buffer_head(const DecoderBuffer *buffer)
{
	return buffer->data + buffer->consumed;
}

size_t
decoder_buffer_available(const DecoderBuffer *buffer)
{
	return buffer->length - buffer->consumed;
}

ConstBuffer<void>
decoder_buffer_read(const DecoderBuffer *buffer)
{
	return {
		decoder_buffer_head(buffer),
		decoder_buffer_available(buffer),
	};
}

ConstBuffer<void>
decoder_buffer_need(DecoderBuffer *buffer, size_t min_size)
{
	while (true) {
		const auto available = decoder_buffer_available(buffer);
		if (available >= min_size)
			return { decoder_buffer_head(buffer), available };

		if (!decoder_buffer_fill(buffer))
			return nullptr;
	}
}

void
decoder_buffer_consume(DecoderBuffer *buffer, size_t nbytes)
{
	/* just move the "consumed" pointer - decoder_buffer_shift()
	   will do the real work later (called by
	   decoder_buffer_fill()) */
	buffer->consumed += nbytes;

	assert(buffer->consumed <= buffer->length);
}

bool
decoder_buffer_skip(DecoderBuffer *buffer, size_t nbytes)
{
	const size_t available = decoder_buffer_available(buffer);
	if (available >= nbytes) {
		decoder_buffer_consume(buffer, nbytes);
		return true;
	}

	decoder_buffer_clear(buffer);
	nbytes -= available;

	return decoder_skip(buffer->decoder, *buffer->is, nbytes);
}
