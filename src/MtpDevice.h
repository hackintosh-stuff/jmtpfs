/*
 * MtpDevice.h
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

#ifndef MTPDEVICE_H_
#define MTPDEVICE_H_

#include "libmtp.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <string.h>
#include <magic.h>
#include <boost/utility/string_ref.hpp>
#include <memory>

#define MAGIC_BUFFER_SIZE 8192

class MtpError : public std::runtime_error
{
public:
	explicit MtpError(const std::string& message, LIBMTP_error_number_t errorCode) : std::runtime_error(message), m_errorCode(errorCode) {}
	LIBMTP_error_number_t ErrorCode() { return m_errorCode; }

protected:
	LIBMTP_error_number_t	m_errorCode;
};

class ExpectedMtpErrorNotFound : public MtpError
{
public:
	ExpectedMtpErrorNotFound() : MtpError("Expected an error condition but found none", LIBMTP_ERROR_GENERAL) {}
};

class MtpErrorCantOpenDevice : public MtpError
{
public:
	MtpErrorCantOpenDevice() : MtpError("Can't open device", LIBMTP_ERROR_GENERAL) {}
};


class MtpDeviceDisconnected : public MtpError
{
public:
	MtpDeviceDisconnected(const std::string& message) : MtpError(message, LIBMTP_ERROR_NO_DEVICE_ATTACHED) {}
};

class MtpDeviceNotFound : public MtpError
{
public:
	MtpDeviceNotFound(const std::string& message) : MtpError(message, LIBMTP_ERROR_GENERAL) {}
};

class MtpStorageNotFound : public MtpError
{
public:
	MtpStorageNotFound(const std::string& message) : MtpError(message, LIBMTP_ERROR_GENERAL) {}
};


#if (0)

#define DEBUG(fmt, ARGS...) \
    fprintf(stderr, "[debug] " fmt "\n", ##ARGS)

#else

#define DEBUG(fmt, ARGS...)

#endif

struct MtpStorageInfo
{
	MtpStorageInfo() = default;

	MtpStorageInfo(uint32_t i, const std::string& s, uint64_t fs, uint64_t mc) :
		id(i), description(s), freeSpaceInBytes(fs), maxCapacity(mc) {
	}

	uint32_t id;
	std::string description;
	uint64_t freeSpaceInBytes;
	uint64_t maxCapacity;
};

struct MtpFileInfo
{
	MtpFileInfo(const boost::string_ref& filename, uint32_t parentId, uint32_t storageId, uint64_t size=0);

	MtpFileInfo(LIBMTP_file_t* info) :
		filename(info->filename),
		fileinfo(info, LIBMTP_file_deleter())
	{ }

	uint32_t& id() { return fileinfo->item_id; }
	const uint32_t& id() const { return fileinfo->item_id; }

	uint32_t& parentId() { return fileinfo->parent_id; }
	const uint32_t& parentId() const { return fileinfo->parent_id; }

	uint32_t& storageId() { return fileinfo->storage_id; }
	const uint32_t& storageId() const { return fileinfo->storage_id; }

	const boost::string_ref& name() const { return filename; }

	LIBMTP_filetype_t& filetype() { return fileinfo->filetype; }
	const LIBMTP_filetype_t& filetype() const { return fileinfo->filetype; }

	uint64_t& filesize() { return fileinfo->filesize; }
	const uint64_t& filesize() const { return fileinfo->filesize; }

	time_t& modificationdate() { return fileinfo->modificationdate; }
	const time_t& modificationdate() const { return fileinfo->modificationdate; }

	operator LIBMTP_file_t*() { return fileinfo.get(); }
	operator const LIBMTP_file_t*() const { return fileinfo.get(); }

	void stat(struct stat& s) const;

	void updateName() { filename = boost::string_ref(fileinfo->filename); }

private:
	struct LIBMTP_file_deleter {
		void operator()(LIBMTP_file_t* info) const {
			LIBMTP_destroy_file_t(info);
		}
	};

private:
	boost::string_ref filename;
	std::shared_ptr<LIBMTP_file_t> fileinfo;
};


class MtpDevice
{
public:
	MtpDevice(LIBMTP_raw_device_t& rawDevice);
	~MtpDevice();

	std::string Get_Modelname();
	std::vector<MtpStorageInfo> GetStorageDevices();
	MtpStorageInfo GetStorageInfo(uint32_t storageId);
	std::vector<MtpFileInfo> GetFolderContents(uint32_t storageId, uint32_t folderId);
	MtpFileInfo GetFileInfo(uint32_t id);
	void GetFile(uint32_t id, int fd);
	void SendFile(LIBMTP_file_t* destination, int fd);
	void CreateFolder(const boost::string_ref& name, uint32_t parentId, uint32_t storageId);
	void DeleteObject(uint32_t id);
	void RenameFile(uint32_t id, const boost::string_ref& name);
	void RenameFile(MtpFileInfo& info, const boost::string_ref& name);
	void SetObjectProperty(uint32_t id, LIBMTP_property_t property, const std::string& value);
	void SetObjectProperty(uint32_t id, LIBMTP_property_t property, const boost::string_ref& value);
	static LIBMTP_filetype_t PropertyTypeFromMimeType(const std::string& mimeType);


protected:
	void CheckErrors(bool throwEvenIfNoError);
	LIBMTP_mtpdevice_t* m_mtpdevice;
	uint32_t		m_busLocation;
	uint8_t			m_devnum;
	magic_t			m_magicCookie;
	char			m_magicBuffer[MAGIC_BUFFER_SIZE];
};



#endif /* MTPDEVICE_H_ */
