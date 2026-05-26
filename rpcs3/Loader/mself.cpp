#include "stdafx.h"

#include "Utilities/File.h"
#include "util/logs.hpp"
#include "Emu/VFS.h"

#include "mself.hpp"

LOG_CHANNEL(mself_log, "MSELF");

bool extract_mself(const std::string& file, const std::string& extract_to)
{
	fs::file mself(file);

	mself_log.notice("Extracting MSELF file '%s' to directory '%s'...", file, extract_to);

	if (!mself)
	{
		mself_log.error("Error opening MSELF file '%s' (%s)", file, fs::g_tls_error);
		return false;
	}

	mself_header hdr{};

	if (!mself.read(hdr))
	{
		mself_log.error("Error reading MSELF header, file is too small. (size=0x%x)", mself.size());
		return false;
	}

	const u64 mself_size = mself.size();
	const u32 hdr_count = hdr.get_count(mself_size);

	if (!hdr_count)
	{
		mself_log.error("Provided file is not an MSELF");
		return false;
	}

	std::vector<mself_record> recs(hdr_count);

	if (!mself.read(recs))
	{
		mself_log.error("Error extracting MSELF records");
		return false;
	}

	std::vector<u8> buffer;

	for (const mself_record& rec : recs)
	{
		// rec.name is a fixed-size char array with no guaranteed NUL terminator;
		// strnlen-bound it before any conversion to string/string_view.
		const std::string raw_name(rec.name, strnlen(rec.name, sizeof(rec.name)));

		// Reject path traversal before forming the extraction path.
		if (raw_name.find("..") != umax || raw_name.find('\\') != umax || (!raw_name.empty() && raw_name.front() == '/'))
		{
			mself_log.error("MSELF: refusing record with unsafe name '%s'", raw_name);
			return false;
		}

		const std::string name = vfs::escape(raw_name, true);

		const u64 pos = rec.get_pos(mself_size);

		if (!pos)
		{
			mself_log.error("Error extracting %s from MSELF", name);
			return false;
		}

		buffer.resize(rec.size);
		mself.seek(pos);
		mself.read(buffer.data(), rec.size);

		if (!fs::create_path(fs::get_parent_dir(extract_to + name)))
		{
			mself_log.error("Error creating directory %s (%s)", fs::get_parent_dir(extract_to + name), fs::g_tls_error);
			return false;
		}

		if (!fs::write_file(extract_to + name, fs::rewrite, buffer))
		{
			mself_log.error("Error creating %s (%s)", extract_to + name, fs::g_tls_error);
			return false;
		}

		mself_log.success("Extracted '%s' to '%s'", name, extract_to + name);
	}

	mself_log.success("Extraction complete!");
	return true;
}
