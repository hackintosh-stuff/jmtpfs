/*
 * MtpNode.cpp
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
#include "MtpNode.h"
#include "MtpRoot.h"
#include "mtpFilesystemErrors.h"
MtpNode::MtpNode(MtpDevice& device, MtpMetadataCache& cache, uint32_t id) : m_device(device), m_cache(cache), m_id(id)
{
}

MtpNode::~MtpNode()
{

}

uint32_t MtpNode::Id()
{
	return m_id;
}

uint32_t MtpNode::GetParentNodeId()
{
	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);

	if (md->parentId() == 0)
		return md->storageId();
	else
		return md->parentId();
}

namespace {
	const std::string this_dir(".");
	const std::string parent_dir("..");
}

void MtpNode::readdir(NameStatHandler&& cb)
{
	cb(this_dir, NULL);
	cb(parent_dir, NULL);
	readDirectory(std::move(cb));
}

void MtpNode::readDirectory(NameStatHandler&& cb)
{
	throw NotADirectory();
}

size_t MtpNode::directorySize()
{
	throw NotADirectory();
}

void MtpNode::Open()
{
	throw NotImplemented("Open");
}

void MtpNode::Close()
{
	throw NotImplemented("Close");
}

int MtpNode::Read(char *buf, size_t size, off_t offset)
{
	throw NotImplemented("Read");
}

void MtpNode::mkdir(const boost::string_ref& name)
{
	throw NotImplemented("mkdir");
}

void MtpNode::Remove()
{
	throw NotImplemented("mkdir");
}


void MtpNode::CreateFile(const boost::string_ref& name)
{
	throw NotImplemented("CreateFile");
}

int MtpNode::Write(const char* buf, size_t size, off_t offset)
{
	throw NotImplemented("Write");
}

void MtpNode::Truncate(off_t length)
{
	throw NotImplemented("Truncate");
}

void MtpNode::Rename(MtpNode& newParent, const boost::string_ref& newName)
{
	throw NotImplemented("Rename");
}

uint32_t MtpNode::FolderId()
{
	throw NotImplemented("FolderId");
}

uint32_t MtpNode::StorageId()
{
	throw NotImplemented("StorageId");
}

MtpStorageInfo MtpNode::GetStorageInfo()
{
	MtpNodeMetadataPtr md = m_cache.getItem(m_id, *this);
	return std::move(MtpRoot(m_device, m_cache).GetStorageInfo(md->storageId()));
}

void MtpNode::statfs(struct statvfs *stat)
{

	MtpStorageInfo storageInfo = GetStorageInfo();

	stat->f_bsize = 512;  // We have to pick some block size, so why not 512?
	stat->f_blocks = storageInfo.maxCapacity / stat->f_bsize;
	stat->f_bfree = storageInfo.freeSpaceInBytes / stat->f_bsize;
	stat->f_bavail = stat->f_bfree;
	stat->f_namemax = MAX_MTP_NAME_LENGTH;

}

std::unique_ptr<MtpNode> MtpNode::Clone()
{
	throw NotImplemented("Clone");
}
