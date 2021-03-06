// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.

// Wrappers for some commonly loaded file types

#pragma once
#include <oCore/filesystem.h>
#include <oString/ini.h>
#include <oString/xml.h>

namespace ouro { namespace filesystem {

inline std::unique_ptr<xml> load_xml(const path& _Path)
{
	scoped_allocation a = load(_Path, load_option::text_read);
	return std::unique_ptr<xml>(new xml(_Path, (char*)a.release(), a.get_deallocate()));
}

inline std::unique_ptr<ini> load_ini(const path& _Path)
{
	scoped_allocation a = load(_Path, load_option::text_read);
	return std::unique_ptr<ini>(new ini(_Path, (char*)a.release(), a.get_deallocate()));
}

}}
