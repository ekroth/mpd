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

/* \file
 *
 * This file contains functions used by the DSF and DSDIFF decoders.
 *
 */

#include "config.h"
#include "DsdLib.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "tag/TagId3.hxx"
#include "util/Error.hxx"

#include <string.h>

#ifdef HAVE_ID3TAG
#include <id3tag.h>
#endif

bool
DsdId::Equals(const char *s) const
{
	assert(s != nullptr);
	assert(strlen(s) == sizeof(value));

	return memcmp(value, s, sizeof(value)) == 0;
}

/**
 * Skip the #input_stream to the specified offset.
 */
bool
dsdlib_skip_to(Decoder *decoder, InputStream &is,
	       uint64_t offset)
{
	if (is.IsSeekable())
		return is.Seek(offset, IgnoreError());

	if (uint64_t(is.GetOffset()) > offset)
		return false;

	return dsdlib_skip(decoder, is, offset - is.GetOffset());
}

/**
 * Skip some bytes from the #input_stream.
 */
bool
dsdlib_skip(Decoder *decoder, InputStream &is,
	    uint64_t delta)
{
	if (delta == 0)
		return true;

	if (is.IsSeekable())
		return is.Seek(is.GetOffset() + delta, IgnoreError());

	if (delta > 1024 * 1024)
		/* don't skip more than one megabyte; it would be too
		   expensive */
		return false;

	return decoder_skip(decoder, is, delta);
}

#ifdef HAVE_ID3TAG
void
dsdlib_tag_id3(InputStream &is,
	       const struct tag_handler *handler,
	       void *handler_ctx, int64_t tagoffset)
{
	assert(tagoffset >= 0);

	if (tagoffset == 0)
		return;

	if (!dsdlib_skip_to(nullptr, is, tagoffset))
		return;

	struct id3_tag *id3_tag = nullptr;
	id3_length_t count;

	/* Prevent broken files causing problems */
	const auto size = is.GetSize();
	const auto offset = is.GetOffset();
	if (offset >= size)
		return;

	count = size - offset;

	/* Check and limit id3 tag size to prevent a stack overflow */
	if (count == 0 || count > 4096)
		return;

	id3_byte_t dsdid3[count];
	id3_byte_t *dsdid3data;
	dsdid3data = dsdid3;

	if (!decoder_read_full(nullptr, is, dsdid3data, count))
		return;

	id3_tag = id3_tag_parse(dsdid3data, count);
	if (id3_tag == nullptr)
		return;

	scan_id3_tag(id3_tag, handler, handler_ctx);

	id3_tag_delete(id3_tag);

	return;
}
#endif
