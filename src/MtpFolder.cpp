/*
 * MtpFolder.cpp
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

#include "MtpFolder.h"
#include "MtpFile.h"
#include "mtpFilesystemErrors.h"
#include "TemporaryFile.h"

MtpFolder::MtpFolder(MtpDevice& device, MtpMetadataCache& cache, uint32_t storageId,
		uint32_t folderId) : MtpNode(device, cache, folderId ? folderId : storageId),
		m_storageId(storageId), m_folderId(folderId)
{

}


void MtpFolder::getChildren(MtpNodeMetadataPtr md)
{
	md->children = std::move(m_device.GetFolderContents(m_storageId, m_folderId ? m_folderId : 0xFFFFFFFFU));
	md->has_children = true;

	for (const auto& i : md->children) {
		m_cache.putItem(std::make_shared<MtpNodeMetadata>(i));
	}
}

MtpNodeMetadataPtr MtpFolder::getMetadata()
{
	MtpNodeMetadataPtr md = std::make_shared<MtpNodeMetadata>(m_device.GetFileInfo(m_id));
	getChildren(md);
	return std::move(md);
}

MtpNodeMetadataPtr MtpFolder::getItem()
{
	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);

	if (!md->has_children && md->from_cache) {
		getChildren(md);
		m_cache.putItem(md);
	}

	return std::move(md);
}

void MtpFolder::getattr(struct stat& info)
{
	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);

	info.st_mode = S_IFDIR | 0755;
	info.st_nlink = 2 + md->children.size();
	info.st_mtime = md->modificationdate();
}

std::unique_ptr<MtpNode> MtpFolder::getNode(const FilesystemPath& path)
{
	MtpNodeMetadataPtr md = getItem();

	auto filename = path.Head();
	FilesystemPath childPath = path.Body();

	for(const auto& i : md->children)
	{
		if (i.name() == filename)
		{
			return getNode(i, childPath);
		}
	}
	throw FileNotFound(path.str());
}

std::unique_ptr<MtpNode> MtpFolder::getNode(const MtpFileInfo& i, const FilesystemPath& childPath)
{
	if (i.filetype() != LIBMTP_FILETYPE_FOLDER)
		return std::unique_ptr<MtpNode>(new MtpFile(m_device, m_cache, i.id()));
	else
	{
		std::unique_ptr<MtpNode> n(new MtpFolder(m_device, m_cache, m_storageId, i.id()));
		if (childPath.Empty())
			return n;
		else
			return n->getNode(childPath);
	}
}

void MtpFolder::readDirectory(NameStatHandler&& cb)
{
	MtpNodeMetadataPtr md = getItem();
	struct stat s;

	for (const auto& i : md->children) {
		i.stat(s);
		cb(i.name(), &s);
	}
}

size_t MtpFolder::directorySize()
{
	return getItem()->children.size();
}

void MtpFolder::Remove()
{
	if (directorySize()>0)
		throw MtpDirectoryNotEmpty();
	uint32_t parentId = GetParentNodeId();
	m_device.DeleteObject(m_id);
	m_cache.clearItem(parentId);
	m_cache.clearItem(m_id);

}

void MtpFolder::mkdir(const boost::string_ref& name)
{
	if (name.length() > MAX_MTP_NAME_LENGTH)
		throw MtpNameTooLong();

	m_device.CreateFolder(name, m_folderId, m_storageId);
	m_cache.clearItem(m_id);
}



void MtpFolder::CreateFile(const boost::string_ref& name)
{
	if (name.length() > MAX_MTP_NAME_LENGTH)
		throw MtpNameTooLong();

	MtpFileInfo newFile(name, m_folderId, m_storageId);
	TemporaryFile empty;
	m_device.SendFile(newFile, empty.FileNo());
	m_cache.clearItem(newFile.id());
	m_cache.clearItem(m_id);
}

uint32_t MtpFolder::FolderId()
{
	return m_folderId;
}

uint32_t MtpFolder::StorageId()
{
	return m_storageId;
}

void MtpFolder::Rename(MtpNode& newParent, const boost::string_ref& newName)
{
	if (newName.length() > MAX_MTP_NAME_LENGTH)
		throw MtpNameTooLong();

	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);

	uint32_t parentId = GetParentNodeId();
	if ((newParent.FolderId() == md->parentId()) && (newParent.StorageId() == m_storageId))
	{
		// we can do a real rename
		m_device.RenameFile(*md, newName);
	}
	else
	{
		// we have to do a copy and delete
		newParent.mkdir(newName);
		std::unique_ptr<MtpNode> destDir(newParent.getNode(FilesystemPath(newName)));

		MtpNodeMetadataPtr md = getItem();

		for (const auto& i : md->children) {
			getNode(i)->Rename(*destDir, i.name());
		}
		Remove();
	}
	m_cache.clearItem(newParent.Id());
	m_cache.clearItem(parentId);

}

