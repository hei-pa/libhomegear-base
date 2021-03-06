/* Copyright 2013-2017 Sathya Laufer
 *
 * libhomegear-base is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * libhomegear-base is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libhomegear-base.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU Lesser General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
*/

#ifndef BASELIBIO_H_
#define BASELIBIO_H_

#include <string>
#include <vector>

namespace BaseLib
{
class SharedObjects;

/**
 * This class provides functions to make your life easier.
 */
class Io
{
public:
	/**
	 * Constructor.
	 * It does nothing. You need to call init() to initialize the object.
	 */
	Io();

	/**
	 * Destructor.
	 * Does nothing.
	 */
	virtual ~Io();

	/**
	 * Initialized the object.
	 *
	 * @param baseLib Pointer to the common base library object.
	 */
	void init(SharedObjects* baseLib);

	/**
	 * Checks if a directory exists.
	 *
	 * @param path The path to the directory.
	 * @return Returns true when the directory exists, or false if the directory couldn't be accessed.
	 */
	static bool directoryExists(std::string path);

	/**
	 * Checks if a path is a directory.
	 *
	 * @param path The path to check.
	 * @param[out] result True when the path is a directory otherwise false
	 * @return Returns 0 on success or -1 on error.
	 */
	static int32_t isDirectory(std::string path, bool& result);

	/**
	 * Creates a new directory. Make sure, the directory doesn't exist.
	 *
	 * @param path The directory to create.
	 * @param mode The creation mode.
	 * @return Returns 0 on success or -1 on error.
	 */
	static bool createDirectory(std::string path, mode_t mode);

	/**
	 * Reads a file and returns the content as a string.
	 *
	 * @param filename The path to the file to read.
	 * @return Returns the content of the file as a string.
	 */
	static std::string getFileContent(std::string filename);

	/**
	 * Reads a file and returns the content as a signed binary array.
	 *
	 * @param filename The path to the file to read.
	 * @param maxBytes Maximum number of bytes to read.
	 * @return Returns the content of the file as a char array.
	 */
	static std::vector<char> getBinaryFileContent(std::string filename, uint32_t maxBytes = 0);

	/**
	 * Reads a file and returns the content as an unsigned binary array.
	 *
	 * @param filename The path to the file to read.
	 * @return Returns the content of the file as an unsigned char array.
	 */
	static std::vector<uint8_t> getUBinaryFileContent(std::string filename);

	/**
	 * Checks if a file exists.
	 *
	 * @param filename The path to the file.
	 * @return Returns true when the file exists, or false if the file couldn't be accessed.
	 */
	static bool fileExists(std::string filename);

	/**
	 * Writes a string to a file. If the file already exists it will be overwritten.
	 *
	 * @param filename The path to the file to write.
	 * @param content The content to write to the file.
	 */
	static void writeFile(std::string& filename, std::string& content);

	/**
	 * Writes binary data to a file. If the file already exists it will be overwritten.
	 *
	 * @param filename The path to the file to write.
	 * @param content The content to write to the file.
	 * @param length The number of bytes to write.
	 */
	static void writeFile(std::string& filename, std::vector<char>& content, uint32_t length);

	/**
	 * Writes binary data to a file. If the file already exists it will be overwritten.
	 *
	 * @param filename The path to the file to write.
	 * @param content The content to write to the file.
	 * @param length The number of bytes to write.
	 */
	static void writeFile(std::string& filename, std::vector<uint8_t>& content, uint32_t length);

	/**
	 * Writes binary data to a file. If the file already exists the data will be appended.
	 *
	 * @param filename The path to the file to write.
	 * @param content The content to write to the file.
	 */
	static void appendToFile(std::string& filename, std::string& content);

	/**
	 * Writes binary data to a file. If the file already exists the data will be appended.
	 *
	 * @param filename The path to the file to write.
	 * @param content The content to write to the file.
	 * @param length The number of bytes to write.
	 */
	static void appendToFile(std::string& filename, std::vector<char>& content, uint32_t length);

	/**
	 * Writes binary data to a file. If the file already exists the data will be appended.
	 *
	 * @param filename The path to the file to write.
	 * @param content The content to write to the file.
	 * @param length The number of bytes to write.
	 */
	static void appendToFile(std::string& filename, std::vector<uint8_t>& content, uint32_t length);

	/**
	 * Returns an array of all files within a path. The files are not prefixed with "path".
	 *
	 * @param path The path to get all files for.
	 * @param recursive Also return files of subdirectories. The files are prefixed with the subdirectory.
	 * @return Returns an array of all file names within path.
	 */
	std::vector<std::string> getFiles(std::string path, bool recursive = false);

	/**
	 * Returns an array of all directories within a path. The directories are not prefixed with "path".
	 *
	 * @param path The path to get all directories for.
	 * @param recursive Also return directories within subdirectories. The directories are prefixed with the subdirectory.
	 * @return Returns an array of all directory names within path.
	 */
	std::vector<std::string> getDirectories(std::string path, bool recursive = false);

	/**
	 * Gets the last modified time of a file.
	 *
	 * @param filename The file to get the last modified time for.
	 * @return The unix time stamp of the last modified time.
	 */
	static int32_t getFileLastModifiedTime(const std::string& filename);

	/**
	 * Copys a file.
	 *
	 * @param source The path to the file.
	 * @param dest The destination path to copy the file to.
	 * @return Returns true on success.
	 */
	bool copyFile(std::string source, std::string dest);

	/**
	 * Moves a file.
	 *
	 * @param source The path to the file.
	 * @param dest The destination path to move the file to.
	 * @return Returns true on success.
	 */
	static bool moveFile(std::string source, std::string dest);

	/**
	 * Deletes a file.
	 *
	 * @param file The file to delete.
	 * @return Returns true on success.
	 */
	static bool deleteFile(std::string file);

	/**
	 * Calculates the SHA-2 SHA-512 of a given file.
	 *
	 * @param file The file to calculate the SHA-512 for.
	 * @return Returns the SHA-512 or an empty string on errors.
	 */
	std::string sha512(std::string file);
private:
	/**
	 * Pointer to the common base library object.
	 */
	BaseLib::SharedObjects* _bl = nullptr;
};
}
#endif
