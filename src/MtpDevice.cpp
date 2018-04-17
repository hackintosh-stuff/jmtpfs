/*
 * MtpDevice.cpp
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
#include "MtpDevice.h"
#include "MtpLibLock.h"
#include "ConnectedMtpDevices.h"
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <magic.h>

namespace {

LIBMTP_file_t* createFileInfo(const boost::string_ref& filename, uint32_t parentId, uint32_t storageId, uint64_t size)
{
	LIBMTP_file_t* fi = LIBMTP_new_file_t();
	fi->filename = strndup(filename.data(), filename.size());
	fi->parent_id = parentId;
	fi->storage_id = storageId;
	fi->filesize = size;

	return fi;
}

}


MtpFileInfo::MtpFileInfo(
const boost::string_ref& filename, uint32_t parentId, uint32_t storageId, uint64_t size) :
	MtpFileInfo(createFileInfo(filename, parentId, storageId, size))
{
}

void MtpFileInfo::stat(struct stat& s) const
{
	memset(&s, 0, sizeof(s));

	if (filetype() == LIBMTP_FILETYPE_FOLDER) {
		s.st_mode = S_IFDIR | 0755;
		s.st_nlink = 2;
	} else {
		s.st_mode = S_IFREG | 0644;
		s.st_nlink = 1;
		s.st_size = filesize();
	}
	s.st_mtime = modificationdate();
}


MtpDevice::MtpDevice(LIBMTP_raw_device_t& rawDevice)
{
MtpLibLock	lock;

	m_mtpdevice = LIBMTP_Open_Raw_Device_Uncached(&rawDevice);
	if (m_mtpdevice == 0)
		throw MtpErrorCantOpenDevice();
	m_busLocation = rawDevice.bus_location;
	m_devnum = rawDevice.devnum;
	LIBMTP_Clear_Errorstack(m_mtpdevice);
	m_magicCookie = magic_open(MAGIC_MIME_TYPE);
	if (m_magicCookie == 0)
		throw std::runtime_error("Couldn't init magic");
	if (magic_load(m_magicCookie, 0))
		throw std::runtime_error(magic_error(m_magicCookie));

}

MtpDevice::~MtpDevice()
{
MtpLibLock	lock;

	LIBMTP_Release_Device(m_mtpdevice);
}

std::string MtpDevice::Get_Modelname()
{
MtpLibLock	lock;

	char* fn = LIBMTP_Get_Modelname(m_mtpdevice);
	if (fn)
	{
		std::string result(fn);
		free(fn);
		return result;
	}
	else
	{
		CheckErrors(false);
		return "";
	}
}


std::vector<MtpStorageInfo> MtpDevice::GetStorageDevices()
{
MtpLibLock	lock;

	DEBUG("GetStorageDevices()");

	if (LIBMTP_Get_Storage(m_mtpdevice, LIBMTP_STORAGE_SORTBY_NOTSORTED))
	{
		CheckErrors(true);
	}

	LIBMTP_devicestorage_t* storage = m_mtpdevice->storage;
	std::vector<MtpStorageInfo> result;
	while(storage)
	{
		result.emplace_back(storage->id, storage->StorageDescription,
				storage->FreeSpaceInBytes, storage->MaxCapacity);
		storage = storage->next;
	}
	return std::move(result);

}

MtpStorageInfo MtpDevice::GetStorageInfo(uint32_t storageId)
{
	std::vector<MtpStorageInfo> storages = std::move(GetStorageDevices());

	for(auto& i : storages) {
		if (i.id == storageId)
			return std::move(i);
	}
	throw MtpStorageNotFound("storage not found");
}

std::vector<MtpFileInfo> MtpDevice::GetFolderContents(uint32_t storageId, uint32_t folderId)
{
MtpLibLock lock;

	DEBUG("GetFolderContents(%u, %u)", storageId, folderId);

	std::vector<MtpFileInfo> result;
	LIBMTP_file_t* file = LIBMTP_Get_Files_And_Folders(m_mtpdevice, storageId, folderId);
	if (file == 0)
	{
		CheckErrors(false);
		return std::move(result);
	}

	size_t files = 0;
	for (LIBMTP_file_t* f = file; f != NULL; f = f->next) {
		files++;
	}
	result.reserve(files);

	for (LIBMTP_file_t* f = file; f != NULL; f = f->next) {
		result.emplace_back(f);
	}
	return std::move(result);
}



MtpFileInfo MtpDevice::GetFileInfo(uint32_t id)
{
MtpLibLock lock;

	DEBUG("GetFileInfo(%u)", id);

	LIBMTP_file_t* fileInfoP = LIBMTP_Get_Filemetadata(m_mtpdevice, id);
	if (fileInfoP==0)
	{
		CheckErrors(true);
	}
	return std::move(MtpFileInfo(fileInfoP));
}


void MtpDevice::GetFile(uint32_t id, int fd)
{
MtpLibLock lock;

	DEBUG("GetFile(%u, %d)", id, fd);

	if (LIBMTP_Get_File_To_File_Descriptor(m_mtpdevice, id, fd,0,0))
		CheckErrors(true);
}

void MtpDevice::CreateFolder(const boost::string_ref& name, uint32_t parentId, uint32_t storageId)
{
MtpLibLock lock;

	std::unique_ptr<char> zname( strndup(name.data(), name.length()) );

	DEBUG("CreateFolder(%s, %u, %u)", zname.get(), parentId, storageId);

	if (LIBMTP_Create_Folder(m_mtpdevice, zname.get(), parentId, storageId)==0)
		CheckErrors(true);
}

void MtpDevice::CheckErrors(bool throwEvenWithNoError)
{
MtpLibLock lock;

	LIBMTP_error_t* errors = LIBMTP_Get_Errorstack(m_mtpdevice);
	if (errors)
	{
		LIBMTP_error_number_t errorCode = errors->errornumber;
		std::string errorText(errors->error_text);
		LIBMTP_Clear_Errorstack(m_mtpdevice);
		switch(errorCode)
		{
		case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
			throw MtpDeviceDisconnected(errorText);
		default:
			throw MtpError(errorText, errorCode);
		}

	}
	LIBMTP_Clear_Errorstack(m_mtpdevice);
	if (throwEvenWithNoError)
		throw ExpectedMtpErrorNotFound();
}

void MtpDevice::DeleteObject(uint32_t id)
{
MtpLibLock lock;

	DEBUG("DeleteObject(%u)", id);

	if (LIBMTP_Delete_Object(m_mtpdevice, id))
		CheckErrors(true);
}

void MtpDevice::SendFile(LIBMTP_file_t* destination, int fd)
{
MtpLibLock lock;

	DEBUG("SendFile(%d)", fd);

const char* mimeType = 0;
	if (destination->filesize > 0)
	{
		// We want to use magic_descriptor here, but there is a bug
		// in magic_descriptor that closes the file descriptor, which
		// then prevents the LIBMTP_Send_File_From_File_Descriptor from working.
		// I tried calling dup on fd and passing the new file descriptor to
		// magic_descriptor, but for some reason that still didn't work.
		// Perhaps because our file descriptors were created with tmpfile
		// there is some other bug that makes the file go away on the first close.
		// So as a work around we copy the beginning of the file into memory and use
		// magic_buffer. Hopefully our buffer is big enough that libmagic has
		// enough data to work its magic.
		lseek(fd, 0, SEEK_SET);
		ssize_t bytesRead = read(fd, m_magicBuffer, MAGIC_BUFFER_SIZE);
		lseek(fd,0,SEEK_SET);
		mimeType = magic_buffer(m_magicCookie, m_magicBuffer, bytesRead);
	}
	if (mimeType)
		destination->filetype = PropertyTypeFromMimeType(mimeType);


	if (LIBMTP_Send_File_From_File_Descriptor(m_mtpdevice, fd, destination, 0,0))
		CheckErrors(true);
}


void MtpDevice::RenameFile(uint32_t id, const boost::string_ref& name)
{
MtpLibLock lock;

	std::unique_ptr<char> zname( strndup(name.data(), name.length()) );

	DEBUG("RenameFile(%u, %s)", id, zname.get());

	LIBMTP_file_t* fileInfo = LIBMTP_Get_Filemetadata(m_mtpdevice, id);
	if (fileInfo==0)
	{
		CheckErrors(true);
	}
	int result = LIBMTP_Set_File_Name(m_mtpdevice, fileInfo, zname.get());
	LIBMTP_destroy_file_t(fileInfo);
	if (result)
		CheckErrors(true);
}

void MtpDevice::RenameFile(MtpFileInfo& info, const boost::string_ref& name)
{
MtpLibLock lock;

	std::unique_ptr<char> zname( strndup(name.data(), name.length()) );

	DEBUG("RenameFile(%u, %s)", info->id(), zname.get());

	int result = LIBMTP_Set_File_Name(m_mtpdevice, info, zname.get());
	if (result)
		CheckErrors(true);

	info.updateName();
}

void MtpDevice::SetObjectProperty(uint32_t id, LIBMTP_property_t property, const std::string& value)
{
	MtpLibLock lock;

	DEBUG("SetObjectProperty(%u ...)", id);

	if (LIBMTP_Set_Object_String(m_mtpdevice, id, property, value.c_str()))
		CheckErrors(true);
}

LIBMTP_filetype_t MtpDevice::PropertyTypeFromMimeType(const std::string& mimeType)
{
	if (mimeType == "video/quicktime")
		return LIBMTP_FILETYPE_QT;
	if (mimeType == "video/x-sgi-movie")
		return LIBMTP_FILETYPE_UNDEF_VIDEO;
	if (mimeType == "video/mp4")
		return LIBMTP_FILETYPE_MP4;
	if (mimeType == "video/3gpp")
		return LIBMTP_FILETYPE_MP4;
	if (mimeType == "audio/mp4")
		return LIBMTP_FILETYPE_M4A;
	if (mimeType == "video/mpeg")
		return LIBMTP_FILETYPE_MPEG;
	if (mimeType == "video/mpeg4-generic")
		return LIBMTP_FILETYPE_MPEG;
	if (mimeType == "audio/x-hx-aac-adif")
		return LIBMTP_FILETYPE_AAC;
	if (mimeType == "audio/x-hx-aac-adts")
		return LIBMTP_FILETYPE_AAC;
	if (mimeType ==  "audio/x-mp4a-latm")
		return LIBMTP_FILETYPE_M4A;
	if (mimeType == "video/x-fli")
		return LIBMTP_FILETYPE_UNDEF_VIDEO;
	if (mimeType == "video/x-flc")
		return LIBMTP_FILETYPE_UNDEF_VIDEO;
	if (mimeType == "video/x-unknown")
		return LIBMTP_FILETYPE_UNDEF_VIDEO;
	if (mimeType == "video/x-ms-asf")
		return LIBMTP_FILETYPE_ASF;
	if (mimeType == "video/x-mng")
		return LIBMTP_FILETYPE_UNDEF_VIDEO;
	if (mimeType == "video/x-jng")
		return LIBMTP_FILETYPE_UNDEF_VIDEO;
	if (mimeType == "video/h264")
		return LIBMTP_FILETYPE_UNDEF_VIDEO;
	if (mimeType == "audio/basic")
		return LIBMTP_FILETYPE_UNDEF_AUDIO;
	if (mimeType == "audio/midi")
		return LIBMTP_FILETYPE_UNDEF_AUDIO;
	if (mimeType == "image/jp2")
		return LIBMTP_FILETYPE_JP2;
	if (mimeType == "audio/x-unknown")
		return LIBMTP_FILETYPE_UNDEF_AUDIO;
	if (mimeType == "audio/x-pn-realaudio")
		return LIBMTP_FILETYPE_UNDEF_AUDIO;
	if (mimeType == "audio/x-mod")
		return LIBMTP_FILETYPE_UNDEF_AUDIO;
	if (mimeType == "audio/x-flac")
		return LIBMTP_FILETYPE_FLAC;
	if (mimeType == "image/tiff")
		return LIBMTP_FILETYPE_TIFF;
	if (mimeType == "image/png")
		return LIBMTP_FILETYPE_PNG;
	if (mimeType == "image/gif")
		return LIBMTP_FILETYPE_GIF;
	if (mimeType == "image/x-ms-bmp")
		return LIBMTP_FILETYPE_BMP;
	if (mimeType == "image/jpeg")
		return LIBMTP_FILETYPE_JPEG;
	if (mimeType =="text/calendar")
		return LIBMTP_FILETYPE_VCALENDAR2;
	if (mimeType =="text/x-vcard")
		return LIBMTP_FILETYPE_VCARD2;
	if (mimeType == "application/x-dosexec")
		return LIBMTP_FILETYPE_WINEXEC;
	if (mimeType == "application/msword")
		return LIBMTP_FILETYPE_DOC;
	if (mimeType == "application/vnd.ms-excel")
		return LIBMTP_FILETYPE_XLS;
	if (mimeType == "application/vnd.ms-powerpoint")
		return LIBMTP_FILETYPE_PPT;
	if (mimeType == "audio/x-wav")
		return LIBMTP_FILETYPE_WAV;
	if (mimeType == "video/x-msvideo")
		return LIBMTP_FILETYPE_AVI;
	if (mimeType == "LIBMTP_FILETYPE_AVI")
		return LIBMTP_FILETYPE_WAV;
	if (mimeType == "text/html")
		return LIBMTP_FILETYPE_HTML;
	if (mimeType == "application/xml")
		return LIBMTP_FILETYPE_TEXT;
	if (mimeType == "application/ogg")
		return LIBMTP_FILETYPE_OGG;
	if (mimeType == "audio/mpeg")
		return LIBMTP_FILETYPE_MP3;
	if (mimeType == "")


	if (mimeType.substr(0,5) == "text/")
		return LIBMTP_FILETYPE_TEXT;
	if (mimeType.substr(0,6) == "video/")
		return LIBMTP_FILETYPE_UNDEF_VIDEO;
	if (mimeType.substr(0,6) == "audio/")
		return LIBMTP_FILETYPE_UNDEF_AUDIO;
	return LIBMTP_FILETYPE_UNKNOWN;
}
