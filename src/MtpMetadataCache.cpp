/*
 * MtpMetadataCache.cpp
 *
 *      Author: Jason Ferrara
 *
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 3 as published by the Free Software Foundation.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02111-1301, USA.
 * licensing@fsf.org
 */
#include "MtpMetadataCache.h"

#include <time.h>
#include <assert.h>

MtpMetadataCacheFiller::~MtpMetadataCacheFiller()
{

}


void MtpMetadataCache::putItem(MtpNodeMetadataPtr data)
{

	clearOld();

	DEBUG("cache put %u '%s'", data->id(), data->name().data());
	CacheEntry newData { data };
	newData.data->from_cache = true;
	newData.whenCreated = time(0);
	m_cacheLookup[data->id()] = m_cache.insert(m_cache.end(), std::move(newData));
}


MtpNodeMetadataPtr MtpMetadataCache::getItem(uint32_t id, MtpMetadataCacheFiller& source)
{

	clearOld();
	cache_lookup_type::iterator i = m_cacheLookup.find(id);
	if (i != m_cacheLookup.end()) {
		DEBUG("cache hit %u '%s'", id, i->second->data->name().data());
		return i->second->data;
	}

	DEBUG("cache miss %u", id);
	CacheEntry newData { source.getMetadata() };
	assert(newData.data->id() == id);
	newData.whenCreated = time(0);

	auto new_i = m_cache.insert(m_cache.end(), newData);
	m_cacheLookup[id] = new_i;
	new_i->data->from_cache = true;

	return newData.data;
}

void MtpMetadataCache::clearItem(uint32_t id)
{

	cache_lookup_type::iterator i = m_cacheLookup.find(id);
	if (i != m_cacheLookup.end())
	{
		m_cache.erase(i->second);
		m_cacheLookup.erase(i);
	}
}

void MtpMetadataCache::clearOld()
{

	time_t now = time(0);
	for(cache_type::iterator i = m_cache.begin(); i != m_cache.end();)
	{
		if ((now - i->whenCreated) > 5)
		{
			m_cacheLookup.erase(i->data->id());
			i = m_cache.erase(i);
		}
		else
			return;
	}
}

MtpLocalFileCopy* MtpMetadataCache::openFile(MtpDevice& device, uint32_t id)
{
	local_file_cache_type::iterator i = m_localFileCache.find(id);
	if (i != m_localFileCache.end())
		return i->second.get();
	MtpLocalFileCopy* newFile = new MtpLocalFileCopy(device, id);
	m_localFileCache[id].reset(newFile);
	return newFile;
}

MtpLocalFileCopy* MtpMetadataCache::getOpenedFile(uint32_t id)
{
	local_file_cache_type::iterator i = m_localFileCache.find(id);
	if (i != m_localFileCache.end())
		return i->second.get();
	else
		return 0;
}

uint32_t MtpMetadataCache::closeFile(uint32_t id)
{
	local_file_cache_type::iterator i = m_localFileCache.find(id);
	if (i != m_localFileCache.end())
	{
		uint32_t newId = i->second->close();
		m_localFileCache.erase(i);
		return newId;
	}
	return id;
}
