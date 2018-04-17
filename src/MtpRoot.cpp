/*
 * MtpRoot.cpp
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
#include "MtpRoot.h"
#include "mtpFilesystemErrors.h"
#include "MtpStorage.h"
#include <limits>

MtpRoot::MtpRoot(MtpDevice& device, MtpMetadataCache& cache) : MtpNode(device, cache, std::numeric_limits<uint32_t>::max())
{
}

void MtpRoot::getattr(struct stat& info)
{
	info.st_mode = S_IFDIR | 0755;
	info.st_nlink = 2 + directorySize();

}

std::unique_ptr<MtpNode> MtpRoot::getNode(const FilesystemPath& path)
{
	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);

	if (path.Empty())
		throw FileNotFound(path.str());
	auto storageName = path.Head();
	for (const auto& i : md->storages)
	{
		if (i.description == storageName)
		{
			std::unique_ptr<MtpNode> storageDevice(new MtpStorage(m_device, m_cache, i.id));
			FilesystemPath childPath = path.Body();
			if (childPath.Empty())
				return storageDevice;
			else
				return storageDevice->getNode(childPath);
		}
	}
	throw FileNotFound(path.str());
}


void MtpRoot::readDirectory(NameStatHandler&& cb)
{
	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);

	for (const auto& i : md->storages)
	{
		cb(i.description, NULL);
	}
}


size_t MtpRoot::directorySize()
{
	return m_cache.getItem(m_id, *this)->storages.size();
}

void MtpRoot::mkdir(const boost::string_ref& name)
{
	throw ReadOnly();
}

void MtpRoot::Remove()
{
	throw ReadOnly();
}

MtpNodeMetadataPtr MtpRoot::getMetadata()
{
	static MtpFileInfo root_info("", 0, 0, 0);

	MtpNodeMetadataPtr md = std::make_shared<MtpNodeMetadata>(root_info);
	md->id() = m_id;
	md->storages = std::move(m_device.GetStorageDevices());
	md->has_storages = true;

	for (const auto& i : md->storages) {
		MtpNodeMetadataPtr smd = std::make_shared<MtpNodeMetadata>(
			MtpFileInfo(i.description, 0, i.id));

		smd->id() = i.id;
		smd->filetype() = LIBMTP_FILETYPE_FOLDER;

		m_cache.putItem(smd);
	}

	return std::move(md);
}

MtpStorageInfo MtpRoot::GetStorageInfo()
{
	return std::move(MtpStorageInfo(0,"",0,0));
}

MtpStorageInfo MtpRoot::GetStorageInfo(uint32_t storageId)
{
	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);

	for(auto& i : md->storages) {
		if (i.id == storageId)
			return i;
	}
	throw MtpStorageNotFound("storage not found");
}

void MtpRoot::statfs(struct statvfs *stat)
{
	size_t totalSize = 0;
	size_t totalFree = 0;

	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);

	for (const auto& i : md->storages)
	{
		totalSize += i.maxCapacity;
		totalFree += i.freeSpaceInBytes;
	}

	stat->f_bsize = 512;  // We have to pick some block size, so why not 512?
	stat->f_blocks = totalSize / stat->f_bsize;
	stat->f_bfree = totalFree / stat->f_bsize;
	stat->f_bavail = stat->f_bfree;
	stat->f_namemax = MAX_MTP_NAME_LENGTH;
}
