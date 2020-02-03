/*
 * Oreka -- A media capture and retrieval platform
 * 
 * Copyright (C) 2005, orecx LLC
 *
 * http://www.orecx.com
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License.
 * Please refer to http://www.gnu.org/copyleft/gpl.html
 *
 */
#pragma warning( disable: 4786 ) // disables truncated symbols in browse-info warning

#include "ConfigManager.h"
#include "MediaChunkFile.h"
#include "Utils.h"

#define MAX_CHUNK_SIZE 100000


MediaChunkFile::MediaChunkFile()
{
//printf("Im MediaChunkFile created\n");
	m_stream = NULL;
	m_mode = READ;
	m_numChunksWritten = 0;
	m_sampleRate = 0;

	m_chunkQueueDataSize = 0;
}

MediaChunkFile::~MediaChunkFile()
{
	Close();
}


void MediaChunkFile::Close()
{
//printf("MediaChunkFile::Close\n");
	if(m_stream)
	{
		FlushToDisk();
		ACE_OS::fclose(m_stream);
		m_stream = NULL;
	}
}

bool MediaChunkFile::FlushToDisk()
{
//printf("___Im Flush To Disk\n");
	bool writeError = false;
	while(m_chunkQueue.size() > 0) // __ run here
	{
//printf("1");
		AudioChunkRef tmpChunk = m_chunkQueue.front();
		m_chunkQueue.pop();
		if(tmpChunk.get() == NULL) // __ not run
		{
//printf("2");
			continue;
		}
		if(tmpChunk->m_pBuffer == NULL) // __ not run
		{
//printf("3");
			continue;
		}

		int numWritten = ACE_OS::fwrite(tmpChunk->GetDetails(), sizeof(AudioChunkDetails), 1, m_stream);
		if(numWritten != 1) // __ not run
		{
//printf("4");
			writeError = true;
			break;
		}
//printf("%s\n",tmpChunk->m_pBuffer);
		numWritten = ACE_OS::fwrite(tmpChunk->m_pBuffer, sizeof(char), tmpChunk->GetNumBytes(), m_stream);
		if(numWritten != tmpChunk->GetNumBytes()) // __ not run
		{
//printf("5");
			writeError = true;
			break;
		}
	}
	m_chunkQueueDataSize = 0;
//printf("__ end Flush To Disk\n");
	return writeError;
}


void MediaChunkFile::WriteChunk(AudioChunkRef chunkRef)
{
//static int n=0;
//printf("MediaChunkFile::WriteChunk %d\n",n++);
//printf("MediaChunkFile::WriteChunk \n");
//printf("__Im MediaChunkFile\n");
	if(chunkRef.get() == NULL) // __ not run
	{
//printf("1 ");
		return;
	}
	if(chunkRef->GetDetails()->m_numBytes == 0)  // __ not run
	{
//printf("2 ");
		return;
	}

	bool writeError = false;

	AudioChunk* pChunk = chunkRef.get();
	m_chunkQueueDataSize += pChunk->GetNumBytes();
	m_chunkQueue.push(chunkRef);
//printf("3 ");
	if(m_chunkQueueDataSize > (unsigned int)(CONFIG.m_captureFileBatchSizeKByte*1024)) // __ run last time or Q size is full
	{
//printf("4 ");
		if (m_stream) // __ run here
		{
//printf("5 ");
			writeError = FlushToDisk();
		}
		else // __ not run
		{
//printf("6 ");
			throw(CStdString("Write attempt on unopened file:")+ m_filename);
		}
	}
	if (writeError)// __ not run
	{
//printf("7 ");
		throw(CStdString("Could not write to file:")+ m_filename);
	}
//printf("__end MediaChunkFile to WriteChunk\n");
}

int MediaChunkFile::ReadChunkMono(AudioChunkRef& chunkRef)
{
//printf("__ Im MediaChunkFile ReadChunkMono\n");
	unsigned int numRead = 0;

	if (m_stream)
	{
//printf("__ MediaChunkFile 1\n");
		chunkRef.reset(new AudioChunk());
		short temp[MAX_CHUNK_SIZE];
		numRead = ACE_OS::fread(temp, sizeof(AudioChunkDetails), 1, m_stream);
		if(numRead == 1)
		{

//printf("__ MediaChunkFile 2\n");
			AudioChunkDetails details;
			memcpy(&details, temp, sizeof(AudioChunkDetails));

			if(details.m_marker != MEDIA_CHUNK_MARKER)
			{
//printf("__ MediaChunkFile 2\n");
				throw(CStdString("Invalid marker in file:")+ m_filename);
			}
			if(details.m_numBytes >= MAX_CHUNK_SIZE)
			{
//printf("__ MediaChunkFile 3\n");
				throw(CStdString("Chunk too big in file:")+ m_filename);
			}
			else
			{

//printf("__ MediaChunkFile 4\n");
				numRead = ACE_OS::fread(temp, sizeof(char), details.m_numBytes, m_stream);
				if(numRead != details.m_numBytes)
				{
//printf("__ MediaChunkFile 5\n");
					throw(CStdString("Incomplete chunk in file:")+ m_filename);
				}
				chunkRef->SetBuffer(temp, details);
			}
		}
	}
	else
	{
//printf("__ MediaChunkFile 6\n");
		throw(CStdString("Read attempt on unopened file:")+ m_filename);
	}
	
	return numRead;
}


void MediaChunkFile::Open(CStdString& filename, fileOpenModeEnum mode, bool stereo, int sampleRate)
{
//printf("MediaChunkFile::Open\n");
//printf("___Open Function\n");
	if(m_sampleRate == 0)
	{
		m_sampleRate = sampleRate;
	}

	if(!m_filename.Equals(filename))
	{
		m_filename = filename + GetExtension();
	}
//printf("m_filename : %s\n",m_filename.c_str());
	m_stream = NULL;
	m_mode = mode;
	if (mode == READ)
	{
//printf("\n___filename : %s\n",m_filename.c_str());
		m_stream = ACE_OS::fopen((PCSTR)m_filename, "rb");
	}
	else
	{
		FileRecursiveMkdir(m_filename, CONFIG.m_audioFilePermissions, CONFIG.m_audioFileOwner, CONFIG.m_audioFileGroup, CONFIG.m_audioOutputPathMcf);
		m_stream = ACE_OS::fopen((PCSTR)m_filename, "wb");

		if(CONFIG.m_audioFilePermissions)
		{
			FileSetPermissions(m_filename, CONFIG.m_audioFilePermissions);
		}

		if(CONFIG.m_audioFileGroup.size() && CONFIG.m_audioFileOwner.size())
		{
			FileSetOwnership(m_filename, CONFIG.m_audioFileOwner, CONFIG.m_audioFileGroup);
		}
	}
	if(!m_stream)
	{
		throw(CStdString("Could not open file: ") + m_filename);
	}
}

CStdString MediaChunkFile::GetExtension()
{
	return ".mcf";
}

void MediaChunkFile::SetNumOutputChannels(int numChan)
{

}
